#include "FavoritesModel.h"
#include "HomeModel.h"
#include "RoomModel.h"
#include "JsonKeys.h"

FavoritesModel::FavoritesModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "favType";
    roles[RoleId] = "ioId";
    roles[RoleIOType] = "ioType";
    roles[RoleName] = "favName";
    setItemRoleNames(roles);
}

QObject *FavoritesModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

bool FavoritesModel::isKnownFavoriteType(int type)
{
    switch (type)
    {
    case Common::FavIO:
    case Common::FavLightsCount:
    case Common::FavShutterCount:
    case Common::FavAudio:
    case Common::FavCamera:
        return true;
    default:
        return false;
    }
}

QVariantList FavoritesModel::save()
{
    QVariantList lst;

    for (int i = 0;i < rowCount();i++)
    {
        QStandardItem *it = dynamic_cast<QStandardItem *>(item(i));
        if (!it) continue;

        //Every favorite is saved the same way, whatever its type: the type
        //itself plus the id of the IO it points at (the camera, the audio
        //player, the all lights counter, ...). Anything else needed to
        //display it is rebuilt from the IO cache at load time.
        QVariantMap vmap;
        vmap["id"] = it->data(RoleId).toString();
        vmap["type"] = it->data(RoleType).toInt();
        lst.append(vmap);
    }

    //Put back the favorites that could not be rebuilt, at the place they were
    //read from, exactly as they were read. They are user data for a favorite
    //type this build does not know how to display (yet): dropping them here
    //would delete them from the settings on the next save.
    for (int i = 0;i < unsupportedFavorites.size();i++)
    {
        int pos = unsupportedFavorites.at(i).first;
        if (pos < 0 || pos > lst.size()) pos = lst.size();
        lst.insert(pos, unsupportedFavorites.at(i).second);
    }

    return lst;
}

void FavoritesModel::load(QVariantList favList)
{
    loaded = false;
    clear();

    int position = 0;
    foreach (QVariant var, favList)
    {
        QVariantMap vmap = var.toMap();

        if (!addFavorite(vmap, position))
            qDebug() << "Failed to add IO: " << vmap["id"].toString();

        position++;
    }
    loaded = true;
}

void FavoritesModel::clear()
{
    unsupportedFavorites.clear();
    QStandardItemModel::clear();
}

bool FavoritesModel::addFavorite(QString ioid, int type)
{
    QVariantMap fav;
    fav["id"] = ioid;
    fav["type"] = type;

    //Appended after everything the model already holds.
    return addFavorite(fav, rowCount() + unsupportedFavorites.size());
}

bool FavoritesModel::addFavorite(const QVariantMap &fav, int position)
{
    QString ioid = fav["id"].toString();
    //"type" is written by save() and only travels through QSettings, so it is
    //always an int this program wrote itself.
    int type = fav["type"].toInt();

    if (isKnownFavoriteType(type))
    {
        //All favorite types point at an IO of the home: FavIO at a regular
        //input/output, FavLightsCount at the "fav_all_lights" pseudo output
        //built by HomeFavModel::load(), FavAudio/FavCamera at the audio
        //player or the camera output. So the row is always rebuilt the same
        //way, from the IO cache.
        IOBase *io = IOCache::Instance().searchInput(ioid);
        if (!io) io = IOCache::Instance().searchOutput(ioid);

        if (io)
        {
            IOBase *newIO = io->cloneIO();
            newIO->setData(ioid, RoleId);
            //RoleType holds the favorite type, the IO type is exposed
            //separately as RoleIOType: that is the role the QML delegates
            //(ItemListView) switch on to pick IOFavAllLights & co.
            newIO->setData(type, RoleType);
            newIO->setData(newIO->get_ioType(), RoleIOType);
            newIO->setData(newIO->get_ioName(), RoleName);
            appendRow(newIO);

            return true;
        }

        //A FavIO whose IO is missing has always been dropped here, and stays
        //dropped: its id is an IO id, so a missing IO means the item is gone
        //from the home and the favorite is stale.
        if (type == Common::FavIO)
            return false;
    }

    //Unknown favorite type, or a non FavIO favorite whose data is not an IO
    //id this build resolves. Keep it out of the rows (nothing could be shown
    //for it) but keep it in the saved list.
    qWarning() << "FavoritesModel: unsupported favorite of type" << type
               << "id" << ioid << ", not displayed but kept in the saved list";
    unsupportedFavorites.append(qMakePair(position, fav));

    return true;
}

void FavoritesModel::delItemFavorite(int idx)
{
    if (idx < 0 || idx >= rowCount()) return;

    removeRow(idx);
}

void FavoritesModel::moveFavorite(int idx, int newidx)
{
    QList<QStandardItem *> it = takeRow(idx);
    insertRow(newidx, it.at(0));
}

HomeFavModel::HomeFavModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent) :
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "roomType";
    roles[RoleHits] = "roomHits";
    roles[RoleName] = "roomName";
    setItemRoleNames(roles);
}

void HomeFavModel::load(const QVariantMap &homeData)
{
    clear();

    if (!homeData.contains(JsonKeys::Home))
    {
        qDebug() << "no home entry";
        return;
    }

    //Create a special favorites Room for adding special items to favorites list (non IO items)
    RoomItem *room = new RoomItem(engine, connection);

    QVariantList lst;

    {
        QVariantMap it;
        it[JsonKeys::Name] = tr("All lights On");
        it[JsonKeys::Type] = "fav_all_lights";
        it[JsonKeys::GuiType] = "fav_all_lights";
        it[JsonKeys::Id] = "fav_all_lights";
        lst.append(it);
    }

    QVariantMap r;

    if (connection->isHttpApiV2())
    {
        QVariantMap items;
        items[JsonKeys::Inputs] = QVariantList();
        items[JsonKeys::Outputs] = lst;
        r[JsonKeys::Items] = items;
    }
    else
    {
        r[JsonKeys::Items] = lst;
    }

    r[JsonKeys::Name] = tr("Special");
    r[JsonKeys::Type] = "fav";
    r[JsonKeys::Hits] = 9999999;

    room->update_roomName(r[JsonKeys::Name].toString());
    room->update_roomType(r[JsonKeys::Type].toString());
    room->update_roomHits(r[JsonKeys::Hits].toString().toInt());
    room->load(r, nullptr, RoomModel::LoadAll);
    appendRow(room);

    //Add normal rooms
    QVariantList rooms = homeData[JsonKeys::Home].toList();
    QVariantList::iterator it = rooms.begin();
    for (;it != rooms.end();it++)
    {
        QVariantMap r = it->toMap();
        RoomItem *room = new RoomItem(engine, connection);
        room->update_roomName(r[JsonKeys::Name].toString());
        room->update_roomType(r[JsonKeys::Type].toString());
        room->update_roomHits(Common::toIntSafe(r[JsonKeys::Hits], 0, "room.hits"));
        room->load(r, nullptr, RoomModel::LoadAll);
        appendRow(room);
    }
}

QObject *HomeFavModel::getRoomModel(int idx) const
{
    RoomItem *it = dynamic_cast<RoomItem *>(item(idx));
    if (!it) return nullptr;
    return it->getRoomModel();
}
