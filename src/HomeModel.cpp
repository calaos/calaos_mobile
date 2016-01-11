#include "HomeModel.h"
#include <QDebug>
#include "RoomModel.h"

HomeModel::HomeModel(QQmlApplicationEngine *eng, CalaosConnection *con, ScenarioModel *scModel, LightOnModel *lModel, QObject *parent) :
    QStandardItemModel(parent),
    engine(eng),
    connection(con),
    scenarioModel(scModel),
    lightOnModel(lModel)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "roomType";
    roles[RoleHits] = "roomHits";
    roles[RoleName] = "roomName";
    roles[RoleLightsCount] = "lights_on_count";
    roles[RoleHasTemp] = "has_temperature";
    roles[RoleCurrentTemp] = "current_temperature";
    setItemRoleNames(roles);

    update_lights_on_count(0);
}

void HomeModel::load(const QVariantMap &homeData)
{
    clear();
    scenarioModel->clear();
    IOCache::Instance().clearCache();
    lightOnModel->clear();

    if (!homeData.contains("home"))
    {
        qDebug() << "no home entry";
        return;
    }

    QVariantList rooms = homeData["home"].toList();
    QVariantList::iterator it = rooms.begin();
    for (;it != rooms.end();it++)
    {
        QVariantMap r = it->toMap();
        RoomItem *room = new RoomItem(engine, connection);
        connect(room, SIGNAL(sig_light_on(IOBase*)), this, SLOT(newlight_on(IOBase*)));
        connect(room, SIGNAL(sig_light_off(IOBase*)), this, SLOT(newlight_off(IOBase*)));

        room->update_roomName(r["name"].toString());
        room->update_roomType(r["type"].toString());
        room->update_roomHits(r["hits"].toString().toInt());
        room->load(r, scenarioModel, RoomModel::LoadNormal);
        appendRow(room);
    }
}

QObject *HomeModel::getRoomModel(int idx) const
{
    RoomItem *it = dynamic_cast<RoomItem *>(item(idx));
    if (!it) return nullptr;
    return it->getRoomModel();
}

void HomeModel::newlight_on(IOBase *io)
{
    update_lights_on_count(get_lights_on_count() + 1);

    lightOnModel->addLight(io);
}

void HomeModel::newlight_off(IOBase *io)
{
    int l = get_lights_on_count() - 1;
    if (l < 0) l = 0;
    update_lights_on_count(l);

    lightOnModel->removeLight(io);
}

RoomItem::RoomItem(QQmlApplicationEngine *eng, CalaosConnection *con):
    QStandardItem(),
    engine(eng),
    connection(con)
{
    update_lights_on_count(0);
    update_has_temperature(false);
    update_current_temperature(0);

    room = new RoomModel(engine, connection, this);
    connect(room, SIGNAL(sig_light_on(IOBase*)), this, SLOT(newlight_on(IOBase*)));
    connect(room, SIGNAL(sig_light_off(IOBase*)), this, SLOT(newlight_off(IOBase*)));
    connect(room, SIGNAL(has_temp_sig(bool)), this, SLOT(has_temperature_slot(bool)));
    connect(room, SIGNAL(temp_changed_sig(double)), this, SLOT(temperature_slot(double)));
    engine->setObjectOwnership(room, QQmlEngine::CppOwnership);
}

QObject *RoomItem::getRoomModel() const
{
    return room;
}

void RoomItem::load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag)
{
    room->load(roomData, scenarioModel, load_flag);
}

void RoomItem::newlight_on(IOBase *io)
{
    update_lights_on_count(get_lights_on_count() + 1);
    emit sig_light_on(io);
}

void RoomItem::newlight_off(IOBase *io)
{
    int l = get_lights_on_count() - 1;
    if (l < 0) l = 0;
    update_lights_on_count(l);
    emit sig_light_off(io);
}

void RoomItem::has_temperature_slot(bool has)
{
    update_has_temperature(has);
}

void RoomItem::temperature_slot(double tmp)
{
    update_current_temperature(tmp);
}

LightOnModel::LightOnModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "ioType";
    roles[RoleHits] = "ioHits";
    roles[RoleName] = "ioName";
    roles[RoleId] = "ioId";
    roles[RoleRoomName] = "roomName";
    setItemRoleNames(roles);
}

QObject *LightOnModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

void LightOnModel::addLight(IOBase *io)
{
    if (onCache.contains(io->get_ioId())) return;

    appendRow(io->cloneIO());
    onCache[io->get_ioId()] = io;

    emit lightCountChanged();
}

void LightOnModel::removeLight(IOBase *io)
{
    for (int i = 0;i < rowCount();i++)
    {
        IOBase *cur = dynamic_cast<IOBase *>(item(i));
        if (cur->get_ioId() == io->get_ioId())
        {
            removeRow(i);
            onCache.remove(io->get_ioId());
            emit lightCountChanged();
            break;
        }
    }
}

int LightOnModel::getLightCount()
{
    return rowCount();
}

QObject *LightOnModel::getQmlCloneModel()
{
    LightOnModel *m = new LightOnModel(engine, connection);

    //The model needs to be sorted by section (room name) again.
    //This has to be done because when a light is added to the model,
    //it's juste appended to the end of the list and qml does not automatically
    //re-order items based on sections. Thus multiple sections are then created.
    //This fixes bug #1
    QHash<QString, QList<IOBase *>> resortedModel;

    for (int i = 0;i < rowCount();i++)
    {
        IOBase *obj = dynamic_cast<IOBase *>(item(i));
        IOBase *newIO = obj->cloneIO();

        if (!resortedModel.contains(newIO->get_room_name()))
        {
            QList<IOBase *> lst;
            lst.append(newIO);
            resortedModel[newIO->get_room_name()] = lst;
        }
        else
        {
            QList<IOBase *> &lst = resortedModel[newIO->get_room_name()];
            lst.append(newIO);
        }
    }

    foreach (QString k, resortedModel.keys())
    {
        const QList<IOBase *> &lst = resortedModel[k];
        foreach (IOBase *newIO, lst)
            m->addLight(newIO);
    }

    engine->setObjectOwnership(m, QQmlEngine::JavaScriptOwnership);
    return m;
}
