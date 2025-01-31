#ifndef CONDITIONS_H
#define CONDITIONS_H

#include <QObject>
#include <QDateTime>

class QNetworkAccessManager;
class QNetworkReply;

class Conditions : public QObject
{
    Q_OBJECT
public:
    explicit Conditions(QObject *parent = nullptr);
    ~Conditions();
    bool isFluxValid();
    bool isKIndexValid();
    bool isAIndexValid();
    int getFlux();
    int getAIndex();
    double getKIndex();

signals:
    void conditionsUpdated();

public slots:
    void update();
    void processReply(QNetworkReply* reply);

private:
    QDateTime flux_last_update;
    QDateTime k_index_last_update;
    QDateTime a_index_last_update;
    int flux;
    int a_index;
    double k_index;

private:
    QNetworkAccessManager* nam;
};

#endif // CONDITIONS_H
