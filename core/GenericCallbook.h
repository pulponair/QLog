#ifndef GENERICCALLBOOK_H
#define GENERICCALLBOOK_H

#include <QObject>
#include <QNetworkReply>

class GenericCallbook : public QObject
{
    Q_OBJECT
public:
    explicit GenericCallbook(QObject *parent = nullptr);
    ~GenericCallbook() {};
    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_USERNAME_KEY;
    const static QString CONFIG_PRIMARY_CALLBOOK_KEY;
    const static QString CONFIG_SECONDARY_CALLBOOK_KEY;
    const static QString CALLBOOK_NAME;

signals:
    void callsignResult(const QMap<QString, QString>& data);
    void lookupError(const QString);
    void loginFailed();
    void callsignNotFound(QString);

public slots:
    virtual void queryCallsign(QString callsign) = 0;

};
#endif // GENERICCALLBOOK_H
