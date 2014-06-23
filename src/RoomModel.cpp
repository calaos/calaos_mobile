#include "RoomModel.h"
#include <QDebug>

RoomModel::RoomModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent) :
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "ioType";
    roles[RoleHits] = "ioHits";
    roles[RoleName] = "ioName";
    roles[RoleId] = "ioId";
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
            IOBase *io = new IOBase(connection, IOBase::IOInput);
            io->load(r);
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
            IOBase *io = new IOBase(connection, IOBase::IOOutput);
            io->load(r);
            appendRow(io);
        }
    }
}

QObject *RoomModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

IOBase::IOBase(CalaosConnection *con, int t):
    QStandardItem(),
    connection(con),
    ioType(t)
{
}

void IOBase::load(QVariantMap &io)
{
    ioData = io;

    update_ioName(ioData["name"].toString());
    update_ioHits(ioData["hits"].toInt());
    update_ioType(Common::IOTypeFromString(ioData["gui_type"].toString()));
    update_ioId(ioData["id"].toString());
}

void IOBase::sendTrue()
{
    connection->sendCommand(ioData["id"].toString(),
            "true",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendFalse()
{
    connection->sendCommand(ioData["id"].toString(),
            "false",
            ioType == IOOutput?"output":"input",
            "set_state");
}

bool IOBase::getStateBool()
{
    if (ioData["state"].toString() == "true")
        return true;
    else
        return false;
}

double IOBase::getStateInt()
{
    return ioData["state"].toDouble();
}

QString IOBase::getStateString()
{
    return ioData["state"].toString();
}
