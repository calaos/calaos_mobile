#include "RoomModel.h"
#include <QDebug>

RoomModel::RoomModel(QObject *parent) :
    QStandardItemModel(parent)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "ioType";
    roles[RoleHits] = "ioHits";
    roles[RoleName] = "ioName";
    setItemRoleNames(roles);
}

void RoomModel::load(QVariantMap &roomData)
{
    clear();

    type = roomData["type"].toString();
    name = roomData["name"].toString();
    hits = roomData["hits"].toString();

    QVariantMap items = roomData["items"].toMap();

    //inputs
    QVariantList inputs = items["inputs"].toList();
    QVariantList::iterator it = inputs.begin();
    for (;it != inputs.end();it++)
    {
        QVariantMap r = it->toMap();

        //TODO:
        //if (r["gui_type"] == "XXX")

        IOBase *io = new IOBase();
        io->update_ioName(r["name"].toString());
        io->update_ioHits(r["hits"].toInt());
        appendRow(io);
    }

    //outputs
    QVariantList outputs = items["outputs"].toList();
    it = outputs.begin();
    for (;it != outputs.end();it++)
    {
        QVariantMap r = it->toMap();

        //TODO:
        //if (r["gui_type"] == "XXX")

        IOBase *io = new IOBase();
        io->update_ioName(r["name"].toString());
        io->update_ioHits(r["hits"].toInt());
        appendRow(io);
    }
}

IOBase::IOBase():
    QStandardItem()
{
}
