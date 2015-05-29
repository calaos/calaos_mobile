#include "RoomModel.h"
#include <QDebug>
#include "HardwareUtils.h"

IOBase *IOCache::searchInput(QString id)
{
    if (inputCache.contains(id))
        return inputCache[id];

    return nullptr;
}

IOBase *IOCache::searchOutput(QString id)
{
    if (outputCache.contains(id))
        return outputCache[id];

    return nullptr;
}

void IOCache::addInput(IOBase *io)
{
    if (io)
        inputCache[io->get_ioId()] = io;
}

void IOCache::addOutput(IOBase *io)
{
    if (io)
        outputCache[io->get_ioId()] = io;
}

void IOCache::delInput(IOBase *io)
{
    if (io)
        inputCache.remove(io->get_ioId());
}

void IOCache::delOutput(IOBase *io)
{
    if (io)
        outputCache.remove(io->get_ioId());
}

void IOCache::clearCache()
{
    foreach (IOBase *io, inputCache)
    {
        delete io;
    }

    foreach (IOBase *io, outputCache)
    {
        delete io;
    }

    inputCache.clear();
    outputCache.clear();
}

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
    roles[RoleRoomName] = "roomName";
    setItemRoleNames(roles);
}

void RoomModel::load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag)
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

        IOBase *io = new IOBase(connection, IOBase::IOInput);
        io->load(r);
        io->update_room_name(name);
        io->checkFirstState();
        IOCache::Instance().addInput(io);

        //create scenario items
        if (r["gui_type"] == "scenario" && scenarioModel)
        {
            IOBase *io = IOCache::Instance().searchInput(r["id"].toString())->cloneIO();
            scenarioModel->appendRow(io);
        }

        //Hide invisible items
        if (r["visible"] != "true")
            continue;

        if (r["gui_type"] == "temp" ||
            r["gui_type"] == "analog_in" ||
            r["gui_type"] == "scenario" ||
            r["gui_type"] == "sctring_in")
        {
            IOBase *io = IOCache::Instance().searchInput(r["id"].toString())->cloneIO();
            appendRow(io);
        }
    }

    //outputs
    QVariantList outputs = items["outputs"].toList();
    it = outputs.begin();
    for (;it != outputs.end();it++)
    {
        QVariantMap r = it->toMap();

        IOBase *io = new IOBase(connection, IOBase::IOOutput);
        connect(io, SIGNAL(light_on(IOBase*)), this, SIGNAL(sig_light_on(IOBase*)));
        connect(io, SIGNAL(light_off(IOBase*)), this, SIGNAL(sig_light_off(IOBase*)));
        io->load(r);
        io->update_room_name(name);
        io->checkFirstState();
        IOCache::Instance().addOutput(io);

        if (load_flag == RoomModel::LoadAll)
        {
            if (r["gui_type"] == "audio_output" ||
                r["gui_type"] == "camera_output" ||
                r["gui_type"] == "fav_all_lights")
            {
                IOBase *io = IOCache::Instance().searchOutput(r["id"].toString())->cloneIO();
                appendRow(io);
            }
        }

        //Hide invisible items
        if (r["visible"] != "true")
            continue;

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
            IOBase *io = IOCache::Instance().searchOutput(r["id"].toString())->cloneIO();
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

void IOBase::load(const QVariantMap &io)
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

void IOBase::checkFirstState()
{
    if (get_ioType() == Common::Light)
    {
        if (getStateBool())
            emit light_on(this);
    }
    else if (get_ioType() == Common::LightDimmer ||
             get_ioType() == Common::LightRgb)
    {
        if (getStateInt() > 0)
            emit light_on(this);
    }
}

IOBase *IOBase::cloneIO() const
{
    IOBase *newIO = new IOBase(connection, ioType);
    newIO->load(ioData);
    newIO->update_room_name(get_room_name());

    return newIO;
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

int IOBase::getStateShutterPos()
{
    QStringList sl = ioData["state"].toString().split(' ');
    if (sl.count() < 1)
        return 0;

    int percent = 0;
    QString status = sl.at(0);
    if (sl.count() > 1)
        percent = sl.at(1).toInt();

    if (percent < 100)
        update_stateShutterBool(true);
    else
        update_stateShutterBool(false);

    if (percent == 0)
        update_stateShutterTxt(tr("State: Opened."));
    else if (percent > 0 && percent < 50)
        update_stateShutterTxt(tr("State: %1% Opened.").arg(percent));
    else if (percent >= 50 && percent < 100)
        update_stateShutterTxt(tr("State: %1% Closed.").arg(percent));

    if (percent == 100)
        update_stateShutterTxt(tr("State: Closed."));

    if (status == "stop" || status == "")
        update_stateShutterTxtAction(tr("Action: stopped."));
    else if (status == "down")
        update_stateShutterTxtAction(tr("Action: Closing..."));
    else if (status == "up")
        update_stateShutterTxtAction(tr("Action: Opening..."));

    return percent;
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
        if (get_ioType() == Common::Light)
        {
            if (getStateBool() != (value == "true"))
            {
                ioData["state"] = value;
                if (value == "true")
                    emit light_on(this);
                else
                    emit light_off(this);
            }
        }
        else if (get_ioType() == Common::LightDimmer ||
                 get_ioType() == Common::LightRgb)
        {
            if ((getStateInt() > 0) != (value.toDouble() > 0))
            {
                ioData["state"] = value;
                if (value.toDouble() > 0)
                    emit light_on(this);
                else
                    emit light_off(this);
            }
        }

        ioData["state"] = value;
        emit stateChange();
    }
    else if (key == "name")
    {
        update_ioName(value);
    }
}

void IOBase::askStateText()
{
    connect(HardwareUtils::Instance(), SIGNAL(dialogTextValid(QString)),
            this, SLOT(textDialogValid(QString)));
    HardwareUtils::Instance()->inputTextDialog(tr("Change value"), tr("Enter new value"));
}

void IOBase::textDialogValid(const QString &text)
{
    disconnect(HardwareUtils::Instance(), SIGNAL(dialogTextValid(QString)),
               this, SLOT(textDialogValid(QString)));
    sendStringValue(text);
}
