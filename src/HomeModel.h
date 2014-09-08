#ifndef HOMEMODEL_H
#define HOMEMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"

class RoomModel;
class ScenarioModel;
class IOBase;

class LightOnModel: public QStandardItemModel
{
    Q_OBJECT
public:
    explicit LightOnModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleType = Qt::UserRole + 1,
        RoleHits,
        RoleName,
        RoleId,
        RoleUnit,
        RoleRW,
        RoleRoomName
    };

    Q_INVOKABLE QObject *getItemModel(int idx);

public slots:
    void addLight(IOBase *io);
    void removeLight(IOBase *io);

private:
    QString name, type, hits;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

class HomeModel: public QStandardItemModel
{
    Q_OBJECT

    QML_READONLY_PROPERTY(int, lights_on_count)

public:
    HomeModel(QQmlApplicationEngine *engine, CalaosConnection *con, ScenarioModel *scModel, LightOnModel *lModel, QObject *parent = 0);

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
    void newlight_on(IOBase *io);
    void newlight_off(IOBase *io);

private:

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
    ScenarioModel *scenarioModel;
    LightOnModel *lightOnModel;
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
    void newlight_on(IOBase *io);
    void newlight_off(IOBase *io);

signals:
    void sig_light_on(IOBase *io);
    void sig_light_off(IOBase *io);

private:
    RoomModel *room = nullptr;
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

#endif // HOMEMODEL_H
