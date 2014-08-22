#include "FavoritesModel.h"
#include "HomeModel.h"
#include "RoomModel.h"

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

QVariantList FavoritesModel::save()
{
    QVariantList lst;

    for (int i = 0;i < rowCount();i++)
    {
        QStandardItem *it = dynamic_cast<QStandardItem *>(item(i));
        if (!it) continue;

        int type = it->data(RoleType).toInt();
        if (type == Common::FavIO)
        {
            QVariantMap vmap;
            vmap["id"] = it->data(RoleId).toString();
            vmap["type"] = type;
            lst.append(vmap);
        }
        else
        {
            qDebug() << "TODO!";
        }
    }

    return lst;
}

void FavoritesModel::load(QVariantList favList)
{
    clear();

    foreach (QVariant var, favList)
    {
        QVariantMap vmap = var.toMap();

        if (!addFavorite(vmap["id"].toString(), vmap["type"].toInt()))
            qDebug() << "Failed to add IO: " << vmap["id"].toString();
    }
}

bool FavoritesModel::addFavorite(QString ioid, int type)
{
    if (type == Common::FavIO)
    {
        IOBase *io = IOCache::Instance().searchInput(ioid);
        if (!io) io = IOCache::Instance().searchOutput(ioid);
        if (!io) return false;

        IOBase *newIO = io->cloneIO();
        newIO->setData(ioid, RoleId);
        newIO->setData(type, RoleType);
        newIO->setData(newIO->get_ioType(), RoleIOType);
        newIO->setData(newIO->get_ioName(), RoleName);
        appendRow(newIO);
    }
    else
    {
        qDebug() << "TODO!";
        return false;
    }

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
