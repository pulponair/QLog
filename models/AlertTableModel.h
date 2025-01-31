#ifndef ALERTTABLEMODEL_H
#define ALERTTABLEMODEL_H

#include <QAbstractTableModel>
#include <data/SpotAlert.h>
#include <QMutex>

class AlertTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    AlertTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent){};
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    void addAlert(SpotAlert entry);
    void clear();
    QString getCallsign(const QModelIndex& index);
    double getFrequency(const QModelIndex& index);

private:
    struct AlertTableRecord
    {
        QDateTime dateTime;
        QStringList ruleName;
        QString callsign;
        double freq;
        QString band;
        QString mode;
        QString comment;
        long long counter;

        bool operator==(const AlertTableRecord &);
        explicit AlertTableRecord(const SpotAlert&);
    };

    QList<AlertTableRecord> alertList;
    QMutex alertListMutex;
};

#endif // ALERTTABLEMODEL_H
