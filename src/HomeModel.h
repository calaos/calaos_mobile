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

    Q_PROPERTY(int lights_count READ getLightCount NOTIFY lightCountChanged)

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

    //this is to get a clone model from qml that would not be updated
    //when any lights are changing. This fixes a bug where items are removed from
    //the listview because you click on the OFF button in lightsOnDetailView
    //To prevent that when opening this view getQmlCloneModel() returns a deep copy
    //of the current model so that the listview does not get any update. User need
    //to close view and reopen it for updated content.
    //Warning: JavaScriptOwnership is set to the returned object (QML is freeing the memory)
    Q_INVOKABLE QObject *getQmlCloneModel();

    int getLightCount();

public slots:
    void addLight(IOBase *io);
    void removeLight(IOBase *io);

signals:
    void lightCountChanged();

private:
    QString name, type, hits;

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;

    QHash<QString, IOBase *> onCache; //cache to allow only unique lights in model
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
        RoleLightsCount,
        RoleHasTemp,
        RoleCurrentTemp,
    };

    void load(const QVariantMap &homeData);

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
    QML_READONLY_PROPERTY_MODEL(bool, has_temperature, HomeModel::RoleHasTemp)
    QML_READONLY_PROPERTY_MODEL(double, current_temperature, HomeModel::RoleCurrentTemp)
public:

    RoomItem(QQmlApplicationEngine *engine, CalaosConnection *con);
    virtual ~RoomItem();

    Q_INVOKABLE QObject *getRoomModel() const;

    void load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag);

public slots:
    void newlight_on(IOBase *io);
    void newlight_off(IOBase *io);
    void has_temperature_slot(bool has);
    void temperature_slot(double tmp);

signals:
    void sig_light_on(IOBase *io);
    void sig_light_off(IOBase *io);

private:
    RoomModel *room;
    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

#endif // HOMEMODEL_H
