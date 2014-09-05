#ifndef HOMEMODEL_H
#define HOMEMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"

class RoomModel;
class ScenarioModel;

class HomeModel: public QStandardItemModel
{
    Q_OBJECT
public:
    HomeModel(QQmlApplicationEngine *engine, CalaosConnection *con, ScenarioModel *scModel, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName
    };

    void load(QVariantMap &homeData);

    Q_INVOKABLE QObject *getRoomModel(int idx) const;

private:

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
    ScenarioModel *scenarioModel;
};

class RoomItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, roomType, HomeModel::RoleType)
    QML_READONLY_PROPERTY_MODEL(int, roomHits, HomeModel::RoleHits)
    QML_READONLY_PROPERTY_MODEL(QString, roomName, HomeModel::RoleName)
public:

    RoomItem(QQmlApplicationEngine *engine, CalaosConnection *con);

    Q_INVOKABLE QObject *getRoomModel() const;

    void load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag);

private:
    RoomModel *room = nullptr;
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

#endif // HOMEMODEL_H
