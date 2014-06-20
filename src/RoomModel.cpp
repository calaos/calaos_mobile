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

        //Hide invisible items
        if (r["visible"] != "true") continue;

        if (r["gui_type"] == "temp" ||
            r["gui_type"] == "analog_in" ||
            r["gui_type"] == "scenario" ||
            r["gui_type"] == "sctring_in")
        {
            IOBase *io = new IOBase();
            io->update_ioName(r["name"].toString());
            io->update_ioHits(r["hits"].toInt());
            io->update_ioType(Common::IOTypeFromString(r["gui_type"].toString()));
            appendRow(io);
        }
    }

    //outputs
    QVariantList outputs = items["outputs"].toList();
    it = outputs.begin();
    for (;it != outputs.end();it++)
    {
        QVariantMap r = it->toMap();

        //Hide invisible items
        if (r["visible"] != "true") continue;

        if (r["gui_type"] == "light" ||
            r["gui_type"] == "light_dimmer" ||
            r["gui_type"] == "light_rgb" ||
            r["gui_type"] == "analog_out" ||
            r["gui_type"] == "shutter" ||
            r["gui_type"] == "shutter_smart" ||
            r["gui_type"] == "var_bool" ||
            r["gui_type"] == "var_int" ||
            r["gui_type"] == "var_string" ||
            r["gui_type"] == "string_out")
        {
            IOBase *io = new IOBase();
            io->update_ioName(r["name"].toString());
            io->update_ioHits(r["hits"].toInt());
            io->update_ioType(Common::IOTypeFromString(r["gui_type"].toString()));
            appendRow(io);
        }
    }
}

IOBase::IOBase():
    QStandardItem()
{
}
