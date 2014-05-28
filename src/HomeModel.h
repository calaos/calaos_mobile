#ifndef HOMEMODEL_H
#define HOMEMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"

class HomeModel: public QStandardItemModel
{
    Q_OBJECT
public:
    HomeModel(QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName
    };

    void load(QVariantMap &homeData);

private:


};

class RoomItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, roomType, HomeModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(int, roomHits, HomeModel::RoleHits)
    QML_READONLY_PROPERTY_MODEL(QString, roomName, HomeModel::RoleName)
public:
    RoomItem();

private:

};

#endif // HOMEMODEL_H
