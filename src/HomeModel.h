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

    QML_READONLY_PROPERTY(int, lights_on_count)

public:
    HomeModel(QQmlApplicationEngine *engine, CalaosConnection *con, ScenarioModel *scModel, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName,
        RoleLightsCount
    };

    void load(QVariantMap &homeData);

    Q_INVOKABLE QObject *getRoomModel(int idx) const;

public slots:
    void newlight_on();
    void newlight_off();

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

    QML_READONLY_PROPERTY_MODEL(int, lights_on_count, HomeModel::RoleLightsCount)
public:

    RoomItem(QQmlApplicationEngine *engine, CalaosConnection *con);

    Q_INVOKABLE QObject *getRoomModel() const;

    void load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag);

public slots:
    void newlight_on();
    void newlight_off();

signals:
    void sig_light_on();
    void sig_light_off();

private:
    RoomModel *room = nullptr;
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

#endif // HOMEMODEL_H
