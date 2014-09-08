#include "HomeModel.h"
#include <QDebug>
#include "RoomModel.h"

HomeModel::HomeModel(QQmlApplicationEngine *eng, CalaosConnection *con, ScenarioModel *scModel, QObject *parent) :
    QStandardItemModel(parent),
    engine(eng),
    connection(con),
    scenarioModel(scModel)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "roomType";
    roles[RoleHits] = "roomHits";
    roles[RoleName] = "roomName";
    setItemRoleNames(roles);

    update_lights_on_count(0);
}

void HomeModel::load(QVariantMap &homeData)
{
    clear();
    scenarioModel->clear();
    IOCache::Instance().clearCache();

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
        connect(room, SIGNAL(sig_light_on()), this, SLOT(newlight_on()));
        connect(room, SIGNAL(sig_light_off()), this, SLOT(newlight_off()));

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

void HomeModel::newlight_on()
{
    update_lights_on_count(get_lights_on_count() + 1);
}

void HomeModel::newlight_off()
{
    int l = get_lights_on_count() - 1;
    if (l < 0) l = 0;
    update_lights_on_count(l);
}

RoomItem::RoomItem(QQmlApplicationEngine *eng, CalaosConnection *con):
    QStandardItem(),
    engine(eng),
    connection(con)
{
    update_lights_on_count(0);
    room = new RoomModel(engine, connection, this);
    connect(room, SIGNAL(sig_light_on()), this, SLOT(newlight_on()));
    connect(room, SIGNAL(sig_light_off()), this, SLOT(newlight_off()));
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

void RoomItem::newlight_on()
{
    update_lights_on_count(get_lights_on_count() + 1);
    emit sig_light_on();
}

void RoomItem::newlight_off()
{
    int l = get_lights_on_count() - 1;
    if (l < 0) l = 0;
    update_lights_on_count(l);
    emit sig_light_off();
}
