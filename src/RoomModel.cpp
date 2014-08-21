#include "RoomModel.h"
#include <QDebug>

ScenarioModel::ScenarioModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleType] = "ioType";
    roles[RoleName] = "ioName";
    roles[RoleId] = "ioId";
    setItemRoleNames(roles);
}

QObject *ScenarioModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

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

void RoomModel::load(QVariantMap &roomData, ScenarioModel *scenarioModel)
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

        //create scenario items
        if (r["gui_type"] == "scenario")
        {
            IOBase *io = new IOBase(connection, IOBase::IOInput);
            io->load(r);
            scenarioModel->appendRow(io);
        }

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
    update_unit(ioData["unit"].toString());
    update_rw(ioData["rw"].toString() == "true");

    //force rw for analog_out to let us use the same qml than var_int
    if (m_ioType == Common::AnalogOut)
        update_rw(true);

    if (ioType == IOInput)
        connect(connection, SIGNAL(eventInputChange(QString,QString,QString)),
                this, SLOT(inputChanged(QString,QString,QString)));
    else
        connect(connection, SIGNAL(eventOutputChange(QString,QString,QString)),
                this, SLOT(outputChanged(QString,QString,QString)));
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

void IOBase::sendInc()
{
    connection->sendCommand(ioData["id"].toString(),
            "inc",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendDec()
{
    connection->sendCommand(ioData["id"].toString(),
            "dec",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendDown()
{
    connection->sendCommand(ioData["id"].toString(),
            "down",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendUp()
{
    connection->sendCommand(ioData["id"].toString(),
            "up",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendStop()
{
    connection->sendCommand(ioData["id"].toString(),
            "stop",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendStringValue(QString value)
{
    connection->sendCommand(ioData["id"].toString(),
            value,
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendIntValue(double value)
{
    connection->sendCommand(ioData["id"].toString(),
            QString("set %1").arg(value),
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

int IOBase::getStateRed()
{
    int state = ioData["state"].toInt();

    int r;
    r = ((state >> 16) * 100) / 255;

    return r;
}

int IOBase::getStateGreen()
{
    int state = ioData["state"].toInt();

    int g;
    g = (((state >> 8) & 0x0000FF) * 100) / 255;

    return g;
}

int IOBase::getStateBlue()
{
    int state = ioData["state"].toInt();

    int b;
    b = ((state & 0x0000FF) * 100) / 255;

    return b;
}

void IOBase::sendRGB(int r, int g, int b)
{
    qDebug() << "Send rgb value: " << r << "," << g << "," << b;
    quint32 val = (((quint32)(r * 255 / 100)) << 16) +
              (((quint32)(g * 255 / 100)) << 8) +
              ((quint32)(b * 255 / 100));

    connection->sendCommand(ioData["id"].toString(),
            QString("set %1").arg(val),
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendValueRed(int value)
{
    sendRGB(value, getStateGreen(), getStateBlue());
}

void IOBase::sendValueGreen(int value)
{
    sendRGB(getStateRed(), value, getStateBlue());
}

void IOBase::sendValueBlue(int value)
{
    sendRGB(getStateRed(), getStateGreen(), value);
}

void IOBase::inputChanged(QString id, QString key, QString value)
{
    if (id != ioData["id"].toString()) return; //not for us

    if (key == "state")
    {
        ioData["state"] = value;
        emit stateChange();
    }
    else if (key == "name")
    {
        update_ioName(value);
    }
}

void IOBase::outputChanged(QString id, QString key, QString value)
{
    if (id != ioData["id"].toString()) return; //not for us

    if (key == "state")
    {
        ioData["state"] = value;
        emit stateChange();
    }
    else if (key == "name")
    {
        update_ioName(value);
    }
}
