#include "RoomModel.h"
#include <QDebug>
#include "IOTypeRegistry.h"
#include "JsonKeys.h"

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

ScenarioModel::ScenarioModel(QQmlApplicationEngine *eng, IOConnection *con, QObject *parent):
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

/* setObjectOwnership() is a static member of QQmlEngine: calling it through
 * the engine pointer only looked like it needed one, and dereferenced null in
 * the models built without an engine (unit tests). Same in the two other
 * getItemModel() below. */
QObject *ScenarioModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

RoomModel::RoomModel(QQmlApplicationEngine *eng, IOConnection *con, QObject *parent) :
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
    roles[RoleStyle] = "ioStyle";
    setItemRoleNames(roles);
}

void RoomModel::load(QVariantMap &roomData, ScenarioModel *scenarioModel, int load_flag)
{
    clear();
    temperatureIo = nullptr;

    type = roomData[JsonKeys::Type].toString();
    name = roomData[JsonKeys::Name].toString();
    hits = roomData[JsonKeys::Hits].toString();

    QVariantMap items = roomData[JsonKeys::Items].toMap();

    QVariantList inputs;
    QVariantList outputs;

    /* Support old protocol and new one. Reading the shape here is fine; what
     * this used to also do was push the answer back into the transport, i.e.
     * the model configured the connection's API version flag (T18). The
     * connection now reads that flag off the get_home payload itself, before
     * handing it over - see CalaosConnection::detectHttpApiV2(). */
    if (items.contains(JsonKeys::Inputs))
    {
        inputs = items[JsonKeys::Inputs].toList();
        outputs = items[JsonKeys::Outputs].toList();
    }
    else
    {
        inputs = roomData[JsonKeys::Items].toList();
        outputs = roomData[JsonKeys::Items].toList();
    }

    //inputs
    QVariantList::iterator it = inputs.begin();
    for (;it != inputs.end();it++)
    {
        QVariantMap r = it->toMap();

        if (r[JsonKeys::GuiType].toString() == "")
            r[JsonKeys::GuiType] = IOTypeRegistry::legacyGuiType(r[JsonKeys::Type].toString());

        IOBase *io = new IOBase(engine, connection, IOBase::IOInput);
        io->load(r);
        io->update_room_name(name);
        io->checkFirstState();
        IOCache::Instance().addInput(io);

        //create scenario items
        if (io->get_ioType() == Common::Scenario && scenarioModel)
        {
            IOBase *io = IOCache::Instance().searchInput(r[JsonKeys::Id].toString())->cloneIO();
            scenarioModel->appendRow(io);
        }

        //Hide invisible items
        if (r[JsonKeys::Visible] != "true")
            continue;

        /* Which types belong to a room view is a property of the type itself:
         * see src/IOTypeRegistry.h. Styled switches (door, smoke...) are
         * covered by the same row as a plain switch was. */
        if (IOTypeRegistry::isRoomVisibleInput(io->get_ioType()))
        {
            IOBase *io = IOCache::Instance().searchInput(r[JsonKeys::Id].toString())->cloneIO();
            appendRow(io);
        }

        if (io->get_ioType() == Common::Temp &&
            !temperatureIo)
        {
            temperatureIo = io;
            emit temp_changed_sig(io->getStateInt());
            emit has_temp_sig(true);

            connect(temperatureIo, &IOBase::destroyed, this, &RoomModel::temperatureIoDestroyed);
            connect(temperatureIo, &IOBase::stateChange, this, &RoomModel::temperatureIoChanged);
        }
    }

    //outputs
    it = outputs.begin();
    for (;it != outputs.end();it++)
    {
        QVariantMap r = it->toMap();

        if (r[JsonKeys::GuiType].toString() == "")
            r[JsonKeys::GuiType] = IOTypeRegistry::legacyGuiType(r[JsonKeys::Type].toString());

        IOBase *io = new IOBase(engine, connection, IOBase::IOOutput);
        connect(io, &IOBase::light_on, this, &RoomModel::sig_light_on);
        connect(io, &IOBase::light_off, this, &RoomModel::sig_light_off);
        io->load(r);
        io->update_room_name(name);
        io->checkFirstState();
        IOCache::Instance().addOutput(io);

        if (load_flag == RoomModel::LoadAll)
        {
            /* Matched on the gui_type and not on the IOType: two of those
             * names are legacy ones with no IOType of their own. */
            if (IOTypeRegistry::isMediaGuiType(r[JsonKeys::GuiType].toString()))
            {
                IOBase *io = IOCache::Instance().searchOutput(r[JsonKeys::Id].toString())->cloneIO();
                appendRow(io);
            }
        }

        //Hide invisible items
        if (r[JsonKeys::Visible] != "true")
            continue;

        //Styled lights (pump, outlet...) are covered by the "light" row.
        if (IOTypeRegistry::isRoomVisibleOutput(io->get_ioType()))
        {
            IOBase *io = IOCache::Instance().searchOutput(r[JsonKeys::Id].toString())->cloneIO();
            appendRow(io);
        }
    }
}

QObject *RoomModel::getItemModel(int idx)
{
    IOBase *obj = dynamic_cast<IOBase *>(item(idx));
    if (obj) QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

void RoomModel::temperatureIoDestroyed()
{
    disconnect(temperatureIo, &IOBase::destroyed, this, &RoomModel::temperatureIoDestroyed);
    disconnect(temperatureIo, &IOBase::stateChange, this, &RoomModel::temperatureIoChanged);

    temperatureIo = nullptr;
    emit has_temp_sig(false);
}

void RoomModel::temperatureIoChanged()
{
    emit temp_changed_sig(temperatureIo->getStateInt());
}

IOBase::IOBase(QQmlApplicationEngine *eng, IOConnection *con, int t):
    QStandardItem(),
    engine(eng),
    connection(con),
    ioType(t)
{
    update_rw(false);
    update_hasStatusInfo(false);
    update_hasStatusConnected(false);
    update_statusConnected(false);
    update_hasStatusBattLevel(false);
    update_statusBattLevel(0);
    update_hasStatusWirelessSignal(false);
    update_statusWirelessSignal(0);
    update_hasStatusUptime(false);
    update_statusUptime(0);
    update_hasStatusIP(false);
    update_hasStatusWifiSSID(false);

    /* IOConnection is a plain interface, so the io signals are reached by name
     * on the QObject that carries them instead of by member pointer (T18: an
     * IO no longer knows the concrete transport class). A wrong signature would
     * otherwise leave an io that never updates, silently: check and shout. */
    QObject *events = connection ? connection->eventSource() : nullptr;
    if (events)
    {
        bool ok;
        if (ioType == IOInput)
            ok = connect(events, SIGNAL(eventInputChange(QString,QString,QString)),
                         this, SLOT(inputChanged(QString,QString,QString)));
        else
            ok = connect(events, SIGNAL(eventOutputChange(QString,QString,QString)),
                         this, SLOT(outputChanged(QString,QString,QString)));

        ok = connect(events, SIGNAL(eventIoStatusChange(QString,QVariantMap)),
                     this, SLOT(ioStatusChanged(QString,QVariantMap))) && ok;

        if (!ok)
            qWarning() << "IOBase: the connection object" << events
                       << "does not expose the expected IOConnection signals,"
                       << "this io will not receive any update";
    }
}

void IOBase::load(const QVariantMap &io)
{
    ioData = io;

    update_ioName(ioData[JsonKeys::Name].toString());
    update_ioHits(Common::toIntSafe(ioData[JsonKeys::Hits], 0, "IOBase.hits"));
    update_ioStyle(ioData[JsonKeys::IoStyle].toString());
    update_ioType(Common::IOTypeFromString(ioData[JsonKeys::GuiType].toString(), get_ioStyle()));
    update_ioId(ioData[JsonKeys::Id].toString());
    update_unit(ioData[JsonKeys::Unit].toString());
    update_rw(ioData[JsonKeys::Rw].toString() == "true");
    update_ioStyle(ioData[JsonKeys::IoStyle].toString());
    update_hasWarning(ioData[JsonKeys::ValueWarning].toString() == "true");

    if (m_ioType == Common::LightRgb)
    {
        if (connection->isHttpApiV2())
            update_rgbColor(QColor(getStateRed(), getStateGreen(), getStateBlue()));
        else
            update_rgbColor(QColor(ioData[JsonKeys::State].toString()));
    }

    //force rw for analog_out to let us use the same qml than var_int
    if (m_ioType == Common::AnalogOut)
        update_rw(true);

    if (io.contains(JsonKeys::StatusInfo))
    {
        QVariantMap statusInfo = io[JsonKeys::StatusInfo].toMap();
        ioStatusChanged(ioData[JsonKeys::Id].toString(), statusInfo);
    }
}

void IOBase::checkFirstState()
{
    /* Only real lights are counted. A pump, an outlet, a boiler and a heater
     * travel as styled lights and render like one, but reporting them would
     * tell the user that lights are on when none is. isBinaryLight and
     * isDimmableLight below answer a different question: how the state reads. */
    if (!IOTypeRegistry::countsAsLight(get_ioType()))
        return;

    if (IOTypeRegistry::isBinaryLight(get_ioType()))
    {
        if (getStateBool())
            emit light_on(this);
    }
    else if (IOTypeRegistry::isDimmableLight(get_ioType()))
    {
        /* Outside the v2 API the state of an RGB io is a color string
         * ("#ff0000"), not a number: feeding it to getStateInt() already
         * yielded 0 (so light_on was never emitted here, the rest of IOBase
         * reads that case back through QColor) and would now make the
         * defensive parsing warn about perfectly valid data. Behaviour
         * unchanged, no false positive in the logs. */
        if (get_ioType() == Common::LightRgb && !connection->isHttpApiV2())
            return;

        if (getStateInt() > 0)
            emit light_on(this);
    }
}

IOBase *IOBase::cloneIO() const
{
    IOBase *newIO = new IOBase(engine, connection, ioType);
    newIO->load(ioData);
    newIO->update_room_name(get_room_name());
    newIO->update_stateShutterBool(get_stateShutterBool());
    newIO->update_stateShutterTxt(get_stateShutterTxt());
    newIO->update_stateShutterTxtAction(get_stateShutterTxtAction());

    return newIO;
}

void IOBase::sendTrue()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "true",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendFalse()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "false",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendInc()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "inc",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendDec()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "dec",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendDown()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "down",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendUp()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "up",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendStop()
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            "stop",
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendStringValue(QString value)
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            value,
            ioType == IOOutput?"output":"input",
            "set_state");
}

void IOBase::sendIntValue(double value)
{
    connection->sendCommand(ioData[JsonKeys::Id].toString(),
            QString("set %1").arg(value),
            ioType == IOOutput?"output":"input",
            "set_state");
}

bool IOBase::getStateBool()
{
    if (ioData[JsonKeys::State].toString() == "true")
        return true;
    else
        return false;
}

double IOBase::getStateInt()
{
    return Common::toDoubleSafe(ioData[JsonKeys::State], 0.0, "IOBase.state");
}

QString IOBase::getStateString()
{
    return ioData[JsonKeys::State].toString();
}

int IOBase::getStateRed()
{
    if (connection->isHttpApiV2())
    {
        int state = Common::toIntSafe(ioData[JsonKeys::State], 0, "IOBase.state(rgb)");

        int r;
        r = state >> 16;

        return r;
    }
    else
    {
        QColor c(ioData[JsonKeys::State].toString());
        return c.red();
    }
}

int IOBase::getStateGreen()
{
    if (connection->isHttpApiV2())
    {
        int state = Common::toIntSafe(ioData[JsonKeys::State], 0, "IOBase.state(rgb)");

        int g;
        g = (state >> 8) & 0x0000FF;

        return g;
    }
    else
    {
        QColor c(ioData[JsonKeys::State].toString());
        return c.green();
    }
}

int IOBase::getStateBlue()
{
    if (connection->isHttpApiV2())
    {
        int state = Common::toIntSafe(ioData[JsonKeys::State], 0, "IOBase.state(rgb)");

        int b;
        b = state & 0x0000FF;

        return b;
    }
    else
    {
        QColor c(ioData[JsonKeys::State].toString());
        return c.blue();
    }
}

void IOBase::sendRGB(int r, int g, int b)
{
    if (connection->isHttpApiV2())
    {
        qDebug() << "Send rgb value: " << r << "," << g << "," << b;
        quint32 val = (((quint32)(r)) << 16) +
                  (((quint32)(g)) << 8) +
                  ((quint32)(b));

        connection->sendCommand(ioData[JsonKeys::Id].toString(),
                QString("set %1").arg(val),
                ioType == IOOutput?"output":"input",
                "set_state");
    }
    else
    {
        QColor c(r, g, b);
        connection->sendCommand(ioData[JsonKeys::Id].toString(),
                QString("set %1").arg(c.name(QColor::HexRgb)),
                QString(),
                "set_state");
    }
}

void IOBase::sendColor(QColor c)
{
    sendRGB(c.red(), c.green(), c.blue());
}

int IOBase::getStateShutterPos()
{
    QStringList sl = ioData[JsonKeys::State].toString().split(' ');
    if (sl.count() < 1)
        return 0;

    int percent = 0;
    QString status = sl.at(0);
    if (sl.count() > 1)
        percent = Common::toIntSafe(sl.at(1), 0, "IOBase.state(shutter percent)");

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
        update_stateShutterTxtAction(tr("Action: Stopped."));
    else if (status == "down")
        update_stateShutterTxtAction(tr("Action: Closing..."));
    else if (status == "up")
        update_stateShutterTxtAction(tr("Action: Opening..."));

    return percent;
}

void IOBase::inputChanged(QString id, QString key, QString value)
{
    if (id != ioData[JsonKeys::Id].toString()) return; //not for us

    if (key == JsonKeys::State)
    {
        ioData[JsonKeys::State] = value;
        emit stateChange();
    }
    else if (key == JsonKeys::Name)
    {
        ioData[JsonKeys::Name] = value;
        update_ioName(value);
    }
    else if (key == JsonKeys::ValueWarning)
    {
        ioData[JsonKeys::ValueWarning] = value;
        update_hasWarning(value == "true");
    }
}

void IOBase::outputChanged(QString id, QString key, QString value)
{
    if (id != ioData[JsonKeys::Id].toString()) return; //not for us

    if (key == JsonKeys::State)
    {
        //Same reading of the type as checkFirstState(), see above: the styled
        //lights are deliberately left out of the counter.
        if (!IOTypeRegistry::countsAsLight(get_ioType()))
        {
            //nothing to report, the unconditional state update below applies
        }
        else if (IOTypeRegistry::isBinaryLight(get_ioType()))
        {
            if (getStateBool() != (value == "true"))
            {
                ioData[JsonKeys::State] = value;
                if (value == "true")
                    emit light_on(this);
                else
                    emit light_off(this);
            }
        }
        else if (IOTypeRegistry::isDimmableLight(get_ioType()))
        {
            if (connection->isHttpApiV2() || get_ioType() == Common::LightDimmer)
            {
                const double newState = Common::toDoubleSafe(value, 0.0, "IOBase.state");
                if ((getStateInt() > 0) != (newState > 0))
                {
                    ioData[JsonKeys::State] = value;
                    if (newState > 0)
                        emit light_on(this);
                    else
                        emit light_off(this);
                }
            }
        }

        ioData[JsonKeys::State] = value;

        if (get_ioType() == Common::LightRgb)
        {
            if (connection->isHttpApiV2())
                update_rgbColor(QColor(getStateRed(), getStateGreen(), getStateBlue()));
            else
            {
                update_rgbColor(QColor(ioData[JsonKeys::State].toString()));

                if (get_rgbColor().red() > 0 ||
                    get_rgbColor().green() > 0 ||
                    get_rgbColor().blue() > 0)
                {
                    emit light_on(this);
                }
                else
                {
                    emit light_off(this);
                }
            }
        }

        emit stateChange();
    }
    else if (key == JsonKeys::Name)
    {
        ioData[JsonKeys::Name] = value;
        update_ioName(value);
    }
}

void IOBase::ioStatusChanged(QString id, QVariantMap statusData)
{
    if (id != ioData[JsonKeys::Id].toString()) return; //not for us

    //If there is any status info, we can enable the status info button
    update_hasStatusInfo(true);

    if (statusData.contains(JsonKeys::BatteryLevel))
    {
        update_hasStatusBattLevel(true);
        update_statusBattLevel(Common::toIntSafe(statusData[JsonKeys::BatteryLevel], 0, "IOBase.status.battery_level"));
    }

    if (statusData.contains(JsonKeys::Connected))
    {
        update_hasStatusConnected(true);
        update_statusConnected(statusData[JsonKeys::Connected].toString() == "true");
    }

    if (statusData.contains(JsonKeys::WirelessSignal))
    {
        update_hasStatusWirelessSignal(true);
        update_statusWirelessSignal(Common::toIntSafe(statusData[JsonKeys::WirelessSignal], 0, "IOBase.status.wireless_signal"));
    }

    if (statusData.contains(JsonKeys::Uptime))
    {
        update_hasStatusUptime(true);
        update_statusUptime(Common::toLongLongSafe(statusData[JsonKeys::Uptime], 0, "IOBase.status.uptime"));
    }

    if (statusData.contains(JsonKeys::IpAddress))
    {
        update_hasStatusIP(true);
        update_statusIP(statusData[JsonKeys::IpAddress].toString());
    }

    if (statusData.contains(JsonKeys::WifiSsid))
    {
        update_hasStatusWifiSSID(true);
        update_statusWifiSSID(statusData[JsonKeys::WifiSsid].toString());
    }
}

bool ScenarioSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    ScenarioModel *scModel = qobject_cast<ScenarioModel *>(sourceModel());
    if (!scModel)
    {
        qWarning() << "ScenarioSortModel: source model is not a ScenarioModel";
        return false;
    }

    IOBase *itemLeft = dynamic_cast<IOBase *>(scModel->itemFromIndex(left));
    IOBase *itemRight = dynamic_cast<IOBase *>(scModel->itemFromIndex(right));

    /* A non-IOBase item (or an invalid index) cannot be ordered: return a
     * stable order instead of dereferencing nullptr. QSortFilterProxyModel
     * calls lessThan() on arbitrary pairs, including while the source model
     * is being rebuilt. */
    if (!itemLeft || !itemRight)
        return itemLeft != nullptr;

    int l = itemLeft->get_ioHits();
    int r = itemRight->get_ioHits();

    if (l == r)
        return itemLeft->get_ioName() < itemRight->get_ioName();

    return l < r;
}

QObject *ScenarioSortModel::getItemModel(int idx)
{
    ScenarioModel *scModel = qobject_cast<ScenarioModel *>(sourceModel());
    if (!scModel)
    {
        qWarning() << "ScenarioSortModel: source model is not a ScenarioModel";
        return nullptr;
    }

    IOBase *obj = dynamic_cast<IOBase *>(scModel->item(indexToSource(idx)));
    if (obj) QQmlEngine::setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}
