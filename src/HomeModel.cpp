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

void HomeModel::setCurrentRoom(int idx)
{
    currentRoomId = idx;
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
    room = new RoomModel(engine, connection, this);
    connect(room, SIGNAL(sig_light_on(IOBase*)), this, SLOT(newlight_on(IOBase*)));
    connect(room, SIGNAL(sig_light_off(IOBase*)), this, SLOT(newlight_off(IOBase*)));
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
    appendRow(io->cloneIO());
}

void LightOnModel::removeLight(IOBase *io)
{
    for (int i = 0;i < rowCount();i++)
    {
        IOBase *cur = dynamic_cast<IOBase *>(item(i));
        if (cur->get_ioId() == io->get_ioId())
        {
            removeRow(i);
            break;
        }
    }
}

void HomeModel::actionIO(QString io_name, QString action, bool plural)
{
    if (plural)
    {
        //TODO, can we make that work like that?
        //remove trailing 's' for plural
        if (io_name.at(io_name.length() - 1) == 's')
            io_name.remove(io_name.length() - 1, 1);
    }

    //try to find io name from the full list
    QList<IOBase *> iolist = IOCache::Instance().lookupName(io_name);

    if (!plural)
    {
        if (iolist.size() > 1)
        {
            qDebug() << "Found " << iolist.size() << " IO with name: " << io_name;
            //TODO, ask which one
        }
        else if (iolist.isEmpty())
        {
            qDebug() << "No IO found with name: " << io_name;
            //TODO, show error
        }
        else
        {
            IOBase *io = iolist.at(0);
            io->processAction(action);
        }
    }

    if (!plural && iolist.size() > 1)
    {
        qDebug() << "Too much IO found with name: " << io_name;
    }

    if (plural)
    {
        foreach (IOBase *io, iolist)
        {
            io->processAction(action);
        }
    }
}

void HomeModel::actionIORoom(QString io_name, QString action, QString room_name, bool plural)
{
    QString rname = Common::removeSpecialChar(room_name).trimmed();
    RoomItem *found = nullptr;

    //find room
    for (int i = 0;i < rowCount();i++)
    {
        RoomItem *it = dynamic_cast<RoomItem *>(item(i));
        QString n = Common::removeSpecialChar(it->get_roomName()).trimmed();
        if (n.contains(rname))
            found = it;
    }

    if (!found)
    {
        qDebug() << "Room not found !";
        return;
    }

    if (plural)
    {
        //TODO, can we make that work like that?
        //remove trailing 's' for plural
        if (io_name.at(io_name.length() - 1) == 's')
            io_name.remove(io_name.length() - 1, 1);
    }
    io_name = Common::removeSpecialChar(io_name).trimmed();
    QList<IOBase *> iolist;

    RoomModel *rmodel = dynamic_cast<RoomModel *>(found->getRoomModel());
    for (int i = 0;i < rmodel->rowCount();i++)
    {
        IOBase *io = dynamic_cast<IOBase *>(rmodel->getItemModel(i));
        QString n = Common::removeSpecialChar(io->get_ioName()).trimmed();
        if (n.contains(io_name))
            iolist.append(io);
    }

    if (!plural)
    {
        if (iolist.size() > 1)
        {
            qDebug() << "Found " << iolist.size() << " IO with name: " << io_name;
            //TODO, ask which one
        }
        else if (iolist.isEmpty())
        {
            qDebug() << "No IO found with name: " << io_name;
            //TODO, show error
        }
        else
        {
            IOBase *io = iolist.at(0);
            io->processAction(action);
        }
    }

    if (!plural && iolist.size() > 1)
    {
        qDebug() << "Too much IO found with name: " << io_name;
    }

    if (plural)
    {
        foreach (IOBase *io, iolist)
        {
            io->processAction(action);
        }
    }
}
