#include <QProgressDialog>
#include <QMessageBox>
#include <QtSql>
#include <QDebug>
#include <QUuid>
#include "core/Migration.h"
#include "core/Cty.h"
#include "core/Sat.h"
#include "debug.h"
#include "data/Data.h"
#include "LogParam.h"

MODULE_IDENTIFICATION("qlog.core.migration");

/**
 * Migrate the database to the latest schema version.
 * Returns true on success.
 */
bool Migration::run() {
    FCT_IDENTIFICATION;

    int currentVersion = getVersion();

    if (currentVersion == latestVersion) {
        qCDebug(runtime) << "Database schema already up to date";
        updateExternalResource();
        return true;
    }
    else if (currentVersion < latestVersion) {
        qCDebug(runtime) << "Starting database migration";
    }
    else {
        qCritical() << "database from the future";
        return false;
    }

    QProgressDialog progress("Migrating the database...", nullptr, currentVersion, latestVersion);
    progress.show();

    while ((currentVersion = getVersion()) < latestVersion) {
        bool res = migrate(currentVersion+1);
        if (!res || getVersion() == currentVersion) {
            progress.close();
            return false;
        }
        progress.setValue(currentVersion);
    }

    progress.close();

    updateExternalResource();

    qCDebug(runtime) << "Database migration successful";

    return true;
}

/**
 * Returns the current user_version of the database.
 */
int Migration::getVersion() {
    FCT_IDENTIFICATION;

    QSqlQuery query("SELECT version FROM schema_versions "
                    "ORDER BY version DESC LIMIT 1");

    int i = query.first() ? query.value(0).toInt() : 0;
    qCDebug(runtime) << i;
    return i;
}

/**
 * Changes the user_version of the database to version.
 * Returns true of the operation was successful.
 */
bool Migration::setVersion(int version) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << version;

    QSqlQuery query;
    if ( ! query.prepare("INSERT INTO schema_versions (version, updated) "
                  "VALUES (:version, datetime('now'))") )
    {
        qWarning() << "Cannot prepare Insert statement";
    }

    query.bindValue(":version", version);

    if (!query.exec()) {
        qWarning() << "setting schema version failed" << query.lastError();
        return false;
    }
    else {
        return true;
    }
}

/**
 * Migrate the database to the given version.
 * Returns true if the operation was successful.
 */
bool Migration::migrate(int toVersion) {
    FCT_IDENTIFICATION;

    qCDebug(runtime) << "migrate to" << toVersion;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qCritical() << "transaction failed";
        return false;
    }

    QString migration_file = QString(":/res/sql/migration_%1.sql").arg(toVersion, 3, 10, QChar('0'));
    bool result = runSqlFile(migration_file);

    result = result && functionMigration(toVersion);

    if (result && setVersion(toVersion) && db.commit()) {
        return true;
    }
    else {
        if (!db.rollback()) {
            qCritical() << "rollback failed";
        }
        return false;
    }
}

bool Migration::runSqlFile(QString filename) {
    FCT_IDENTIFICATION;

    qCDebug(function_parameters) << filename;

    QFile sqlFile(filename);
    sqlFile.open(QIODevice::ReadOnly | QIODevice::Text);

    QStringList sqlQueries = QTextStream(&sqlFile).readAll().split('\n').join("").split(';');
    qCDebug(runtime) << sqlQueries;

    foreach (QString sqlQuery, sqlQueries) {
        if (sqlQuery.trimmed().isEmpty()) {
            continue;
        }

        qCDebug(runtime) << sqlQuery;

        QSqlQuery query;
        if (!query.exec(sqlQuery))
        {
            qCDebug(runtime) << query.lastError();
            return false;
        }
        query.finish();
    }

    return true;
}

bool Migration::functionMigration(int version)
{
    FCT_IDENTIFICATION;

    bool ret = false;
    switch ( version )
    {
    case 4:
        ret = fixIntlFields();
        break;
    case 6:
        ret = insertUUID();
        break;
    default:
        ret = true;
    }

    return ret;
}

int Migration::tableRows(QString name) {
    FCT_IDENTIFICATION;
    int i = 0;
    QSqlQuery query(QString("SELECT count(*) FROM %1").arg(name));
    i = query.first() ? query.value(0).toInt() : 0;
    qCDebug(runtime) << i;
    return i;
}

bool Migration::updateExternalResource() {
    FCT_IDENTIFICATION;

    Cty cty;
    Sat sats;

    QProgressDialog progress("Updating DXCC entities...", nullptr, 0, Cty::MAX_ENTITIES);

    QObject::connect(&cty, &Cty::progress, &progress, &QProgressDialog::setValue);
    QObject::connect(&cty, &Cty::finished, &progress, &QProgressDialog::done);
    QObject::connect(&cty, &Cty::noUpdate, &progress, &QProgressDialog::cancel);

    QObject::connect(&sats, &Sat::progress, &progress, &QProgressDialog::setValue);
    QObject::connect(&sats, &Sat::finished, &progress, &QProgressDialog::done);
    QObject::connect(&sats, &Sat::noUpdate, &progress, &QProgressDialog::cancel);

    cty.update();

    if ( progress.wasCanceled() )
    {
        qCDebug(runtime) << "Update DXCC was canceled";
    }
    else
    {
        progress.show();

        if ( !progress.exec() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("DXCC update failed."));
            return false;
        }
    }

    progress.reset();
    progress.setLabelText("Updating Sats Info...");
    progress.setMinimum(0);
    progress.setMaximum(Sat::MAX_ENTITIES);

    progress.show();

    sats.update();

    if ( progress.wasCanceled() )
    {
        qCDebug(runtime) << "Update SATs was canceled";
    }
    else
    {
        if ( !progress.exec() )
        {
            QMessageBox::warning(nullptr, QMessageBox::tr("QLog Warning"),
                                 QMessageBox::tr("Sats Info update failed."));
            return false;
        }
    }
    return true;
}

/* Fixing error when QLog stored UTF characters to non-Intl field of ADIF (contact) table */
/* The fix has two steps
 * 1) Update contact to move all non-intl to intl fields
 * 2) transform intl field to non-intl field by calloni removeAccents
 */
bool Migration::fixIntlFields()
{
    FCT_IDENTIFICATION;

    QSqlQuery query;
    QSqlQuery update;

    if ( !query.prepare( "SELECT id, name, name_intl, "
                         "           qth, qth_intl, "
                         "           comment, comment_intl, "
                         "           my_antenna, my_antenna_intl,"
                         "           my_city, my_city_intl,"
                         "           my_rig, my_rig_intl,"
                         "           my_sig, my_sig_intl,"
                         "           my_sig_info, my_sig_info_intl,"
                         "           sig, sig_intl,"
                         "           sig_info, sig_info_intl "
                         " FROM contacts" ) )
    {
        qWarning()<< " Cannot prepare a migration script - fixIntlField 1";
        return false;
    }

    if( !query.exec() )
    {
        qWarning()<< "Cannot exec a migration script - fixIntlFields 1";
        return false;
    }

    if ( !update.prepare("UPDATE contacts SET name    = :name,"
                         "                    qth     = :qth, "
                         "                    comment = :comment,"
                         "                    my_antenna = :my_antenna,"
                         "                    my_city = :my_city,"
                         "                    my_rig = :my_rig,"
                         "                    my_sig = :my_sig,"
                         "                    my_sig_info = :my_sig_info,"
                         "                    sig = :sig,"
                         "                    sig_info = :sig_info "
                         "WHERE id = :id") )
    {
        qWarning()<< " Cannot prepare a migration script - fixIntlField 2";
        return false;
    }

    while( query.next() )
    {
        update.bindValue(":id", query.value("id").toInt());
        update.bindValue(":name",       fixIntlField(query, "name", "name_intl"));
        update.bindValue(":qth",        fixIntlField(query, "qth", "qth_intl"));
        update.bindValue(":comment",    fixIntlField(query, "comment", "comment_intl"));
        update.bindValue(":my_antenna", fixIntlField(query, "my_antenna", "my_antenna_intl"));
        update.bindValue(":my_city",    fixIntlField(query, "my_city", "my_city_intl"));
        update.bindValue(":my_rig",     fixIntlField(query, "my_rig", "my_rig_intl"));
        update.bindValue(":my_sig",     fixIntlField(query, "my_sig", "my_sig_intl"));
        update.bindValue(":my_sig_info",fixIntlField(query, "my_sig_info", "my_sig_info_intl"));
        update.bindValue(":sig",        fixIntlField(query, "sig", "sig_intl"));
        update.bindValue(":sig_info",   fixIntlField(query, "sig_info", "sig_info_intl"));

        if ( !update.exec())
        {
            qWarning() << "Cannot exec a migration script - fixIntlFields 2";
            return false;
        }
    }

    return true;
}

bool Migration::insertUUID()
{
    FCT_IDENTIFICATION;

    return LogParam::setParam("logid", QUuid::createUuid().toString());
}

QString Migration::fixIntlField(QSqlQuery &query, const QString &columName, const QString &columnNameIntl)
{
    FCT_IDENTIFICATION;

    QString retValue;

    QString fieldValue = query.value(columName).toString();

    if ( !fieldValue.isEmpty() )
    {
        retValue = Data::removeAccents(fieldValue);
    }
    else
    {
        retValue = Data::removeAccents(query.value(columnNameIntl).toString());
    }

    return retValue;
}
