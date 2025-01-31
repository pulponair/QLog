#ifndef HAMQTH_H
#define HAMQTH_H

#include <QObject>
#include <QString>
#include "core/GenericCallbook.h"

class QNetworkAccessManager;
class QNetworkReply;

class HamQTH : public GenericCallbook
{
    Q_OBJECT

public:
    explicit HamQTH(QObject *parent = nullptr);
    ~HamQTH();

    const static QString CALLBOOK_NAME;
    static const QString getUsername();
    static const QString getPassword();

    static void saveUsernamePassword(const QString&, const QString&);

public slots:
    void queryCallsign(QString callsign) override;

private slots:
    void processReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* nam;
    QString sessionId;
    QString queuedCallsign;
    bool incorrectLogin;
    QString lastSeenPassword;

    void authenticate();

    const static QString SECURE_STORAGE_KEY;
    const static QString CONFIG_USERNAME_KEY;
};

#endif // HAMQTH_H
