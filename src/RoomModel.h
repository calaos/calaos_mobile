#ifndef ROOMMODEL_H
#define ROOMMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>

class RoomModel: public QStandardItemModel
{
    Q_OBJECT
public:
    explicit RoomModel(QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName
    };

    void load(QVariantMap &roomData);

private:
    QString name, type, hits;

};

class IOBase: public QObject, public QStandardItem
{
    Q_OBJECT

    //QML_READONLY_PROPERTY_MODEL(QString, roomType, HomeModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(int, ioHits, RoomModel::RoleHits)
    QML_READONLY_PROPERTY_MODEL(QString, ioName, RoomModel::RoleName)

public:
    IOBase();

    //void load(QVariantMap &roomData);
};

#endif // ROOMMODEL_H
