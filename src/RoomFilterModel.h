#ifndef ROOMFILTERMODEL_H
#define ROOMFILTERMODEL_H

#include <QObject>
#include <QSortFilterProxyModel>
#include "qqmlhelpers.h"
#include "Common.h"

class IOBase;

class RoomFilterModel: public QSortFilterProxyModel
{
    Q_OBJECT

    QML_WRITABLE_PROPERTY(QObject *, source)
    QML_WRITABLE_PROPERTY(Common::FilterModelType, filter)

    //if true, add scenario items to left/right
    //property not used with FilterScenario
    QML_WRITABLE_PROPERTY(bool, scenarioVisible)

public:
    explicit RoomFilterModel(QObject *parent = 0);

    Q_INVOKABLE int indexToSource(int idx);
    Q_INVOKABLE int indexFromSource(int idx);

    Q_INVOKABLE QObject *getItemModel(int idx);

private slots:
    void resetCache();

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;
    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

    QHash<QString, IOBase *> leftCache, rightCache;
    QList<IOBase *> shutters, lights, temps, other;
};

#endif // ROOMFILTERMODEL_H
