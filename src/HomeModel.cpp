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
}

void HomeModel::load(QVariantMap &homeData)
{
    clear();

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
        room->update_roomName(r["name"].toString());
        room->update_roomType(r["type"].toString());
        room->update_roomHits(r["hits"].toString().toInt());
        room->load(r, scenarioModel);
        appendRow(room);
    }
}

QObject *HomeModel::getRoomModel(int idx) const
{
    RoomItem *it = dynamic_cast<RoomItem *>(item(idx));
    if (!it) return NULL;
    return it->getRoomModel();
}

RoomItem::RoomItem(QQmlApplicationEngine *eng, CalaosConnection *con):
    QStandardItem(),
    engine(eng),
    connection(con)
{
    room = new RoomModel(engine, connection, this);
    engine->setObjectOwnership(room, QQmlEngine::CppOwnership);
}

QObject *RoomItem::getRoomModel() const
{
    return room;
}

void RoomItem::load(QVariantMap &roomData, ScenarioModel *scenarioModel)
{
    room->load(roomData, scenarioModel);
}
