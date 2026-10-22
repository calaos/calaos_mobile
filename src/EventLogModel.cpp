#include "EventLogModel.h"
#include "RoomModel.h"
#include "IOTypeRegistry.h"
#include "JsonKeys.h"


EventLogModel::EventLogModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleTitle] = "evTitle";
    roles[RoleDate] = "evDate";
    roles[RoleTime] = "evTime";
    roles[RoleType] = "evType";
    roles[RoleIconSource] = "evIconSource";
    roles[RoleRoomName] = "evRoomName";
    roles[RoleNotifText] = "evNotifText";
    roles[RoleNotifHasPic] = "evHasPicture";
    roles[RoleNotifPicUrl] = "evPictureUrl";
    roles[RoleActionText] = "evActionText";
    setItemRoleNames(roles);

    connect(connection, &CalaosConnection::eventLogPageFinished,
            this, &EventLogModel::pageFinished);
    connect(connection, &CalaosConnection::connectionStateChanged,
            this, &EventLogModel::connectionStateChanged);

    requestTimer.setSingleShot(true);
    connect(&requestTimer, &QTimer::timeout, this, &EventLogModel::requestTimedOut);
}

QObject *EventLogModel::getEventModel(int idx) const
{
    EventLogItem *it = dynamic_cast<EventLogItem *>(item(idx));
    if (it) engine->setObjectOwnership(it, QQmlEngine::CppOwnership);
    return it;
}

void EventLogModel::requestPage(int page, bool replace)
{
    const QString id = connection->requestEventLogPage(page, DEFAULT_PER_PAGE);
    if (id.isEmpty())
    {
        //No session up: nothing was sent, so nothing is waited for. The next
        //login refreshes (Application::homeLoaded()).
        qDebug() << "Event log: no session, page" << page << "not requested";
        return;
    }

    pendingId = id;
    pendingPage = page;
    pendingReplace = replace;
    requestTimer.start(requestTimeoutMs);
}

void EventLogModel::abandonRequest()
{
    if (pendingId.isEmpty())
        return;

    connection->cancelEventLogRequest(pendingId);
    pendingId.clear();
    requestTimer.stop();
}

void EventLogModel::loadMore()
{
    qDebug() << "Load more... rowCount:" << rowCount();
    if (isLoading() || atEnd) return;

    requestPage(nextPage, false);
}

void EventLogModel::refresh()
{
    qDebug() << "refresh...";

    //Whatever is in flight is superseded: an answer to it is ignored.
    abandonRequest();
    requestPage(0, true);
}

void EventLogModel::reset()
{
    abandonRequest();
    //Paging first: the view reacts to clear() (atYEnd -> loadMore()).
    shownIds.clear();
    nextPage = 0;
    atEnd = false;
    clear();
}

void EventLogModel::pageFinished(const QString &requestId, bool ok, const QVariantMap &data)
{
    if (requestId != pendingId)
        return; //an abandoned request, or not ours

    requestTimer.stop();

    if (!ok)
    {
        pendingId.clear();
        //The rows stay as they are; the next refresh() or loadMore() asks
        //again.
        qWarning() << "Event log page" << pendingPage << "not loaded:"
                   << data.value(JsonKeys::Error).toString();
        return;
    }

    //The request stays in flight until its rows are in: the view reacts to
    //the rows changing (clear() puts the list at its end, and
    //EventLogView.qml calls loadMore() on atYEnd), and a request started
    //from there would ask for a page computed from the previous paging.
    const QVariantList events = data.value(JsonKeys::Events).toList();
    if (pendingReplace)
    {
        shownIds.clear();
        clear();
    }
    appendEvents(events);

    nextPage = pendingPage + 1;
    //The last page is the one shorter than asked for, or the one total_page
    //says is last: nothing is requested past it.
    atEnd = events.count() < DEFAULT_PER_PAGE ||
            (data.contains(JsonKeys::TotalPage) &&
             nextPage >= Common::toIntSafe(data.value(JsonKeys::TotalPage), 0, "eventlog.total_page"));
    pendingId.clear();
}

void EventLogModel::appendEvents(const QVariantList &events)
{
    for (const QVariant &v: events)
    {
        const QVariantMap e = v.toMap();
        //calaos_server sends the uuid of an event as "id"
        //(HistEvent::toJson); "uuid" is only the request parameter.
        const QString id = e.value(JsonKeys::Id).toString();
        if (!id.isEmpty())
        {
            if (shownIds.contains(id))
                continue;
            shownIds.insert(id);
        }

        EventLogItem *it = new EventLogItem(engine, connection);
        it->load(e);
        appendRow(it);
    }
}

void EventLogModel::connectionStateChanged(int state)
{
    //Whatever was in flight is lost with the session: stop waiting, the
    //refresh after the next login sends a new request. The rows are kept
    //(stale is better than empty) until that refresh replaces them.
    if (state != ReconnectPolicy::Connected)
        abandonRequest();
}

void EventLogModel::requestTimedOut()
{
    qWarning() << "Event log page" << pendingPage << "not answered after"
               << requestTimeoutMs << "ms, giving up on" << pendingId;
    abandonRequest();
}

QObject *EventLogModel::loadEvent(QString uuid)
{
    EventLogItem *it = new EventLogItem(engine, connection);
    it->load(uuid);
    return it;
}

EventLogItem::EventLogItem(QQmlApplicationEngine *eng, CalaosConnection *con):
    QObject(nullptr),
    engine(eng),
    connection(con)
{
    update_loading(false);
}

void EventLogItem::load(const QVariantMap &data)
{
    /* Roles that only some branches below fill (unknown event type, IO not in
     * the cache, IO type without an icon) must still hold a string: an unset
     * role reads as undefined in QML, and since Qt 6.12 undefined passed to a
     * QString parameter (calaosApp.getPictureSized(evIconSource)) arrives as
     * the string "undefined" instead of an empty string. */
    update_evIconSource(QString());
    update_evActionText(QString());
    update_evRoomName(QString());

    if (data[JsonKeys::EventType] == "3")
    {
        update_evType(Common::EventIoChanged);
        update_evTitle(tr("Appliance change"));
    }
    else if (data[JsonKeys::EventType] == "22")
    {
        update_evType(Common::EventPush);
        update_evTitle(tr("Push Notification"));
        update_evIconSource("icon_notif");

        QVariantMap e = data[JsonKeys::EventRaw].toMap();
        update_evNotifText(e[JsonKeys::Message].toString());

        if (e[JsonKeys::PicUid].toString() == "")
        {
            update_evPictureUrl(QString());
            update_evHasPicture(false);
        }
        else
        {
            update_evPictureUrl(connection->getNotifPictureUrl(e[JsonKeys::PicUid].toString()));
            update_evHasPicture(true);
        }
    }
    else
    {
        update_evType(Common::EventUnknown);
        update_evTitle(tr("Unknown event!"));
    }

    QDateTime dt = QDateTime::fromString(data[JsonKeys::CreatedAt].toString(),
            "yyyy-MM-dd hh:mm:ss");
    dt = QDateTime(dt.date(), dt.time(), Qt::UTC).toLocalTime();

    if (dt.date() == QDate::currentDate())
        update_evDate(tr("Today"));
    else
        update_evDate(dt.date().toString(QLocale::system().dateFormat(QLocale::ShortFormat)));

    update_evTime(dt.time().toString("hh:mm:ss"));

    QString ioId = data[JsonKeys::IoId].toString();
    IOBase *io = EventLogIO::resolve<IOBase>(
        [](const QString &i) { return IOCache::Instance().searchInput(i); },
        [](const QString &i) { return IOCache::Instance().searchOutput(i); },
        ioId);
    if (io)
    {
        update_evTitle(io->get_ioName());
        update_evRoomName(io->get_room_name());

        /* Lights whose state is a boolean: the plain one and the styled ones
         * (pump, outlet, boiler, heater), which only differ by the icon they
         * use. Both the list and the icon name come from the registry, see
         * src/IOTypeRegistry.h. */
        if (IOTypeRegistry::isBinaryLight(io->get_ioType()))
        {
            const QString style = IOTypeRegistry::styleName(io->get_ioType());

            if (data[JsonKeys::IoState].toString() == "true")
            {
                update_evIconSource(QStringLiteral("icon_%1_on").arg(style));
                update_evActionText(tr("On"));
            }
            else
            {
                update_evIconSource(QStringLiteral("icon_%1_off").arg(style));
                update_evActionText(tr("Off"));
            }
        }
        else if (IOTypeRegistry::isDimmableLight(io->get_ioType()))
        {
            if (Common::toDoubleSafe(data[JsonKeys::IoState], 0.0, "event.io_state") > 0)
            {
                update_evIconSource("icon_light_on");
                update_evActionText(tr("On"));
            }
            else
            {
                update_evIconSource("icon_light_off");
                update_evActionText(tr("Off"));
            }
        }
        else if (io->get_ioType() == Common::Temp)
        {
            update_evIconSource("icon_temp");
            update_evActionText(tr("Temp changed"));
        }
        else if (IOTypeRegistry::category(io->get_ioType()) == IOTypeRegistry::Category::Shutter)
        {
            if (data[JsonKeys::IoState].toString() == "true")
            {
                update_evIconSource("icon_shutter_on");
                update_evActionText(tr("Opened"));
            }
            else
            {
                update_evIconSource("icon_shutter_off");
                update_evActionText(tr("Closed"));
            }
        }
        else if (io->get_ioType() == Common::Scenario)
        {
            update_evIconSource("icon_scenario");
            update_evActionText(tr("Started"));
        }
        else
        {
            update_evIconSource(QString());
            update_evActionText(QString());
        }
    }
}

void EventLogItem::load(const QString &uuid)
{
    requestId = connection->requestEventLogEvent(uuid);
    if (requestId.isEmpty())
    {
        //No session up: nothing to wait for.
        update_loading(false);
        return;
    }

    update_loading(true);
    connect(connection, &CalaosConnection::eventLogEventFinished,
            this, &EventLogItem::eventFinished, Qt::UniqueConnection);
}

void EventLogItem::eventFinished(const QString &id, bool ok, const QVariantMap &data)
{
    //Only the answer to this item's own request (EVLOG-1): a page, or the
    //event another item asked for, used to be taken for it.
    if (id != requestId)
        return;

    disconnect(connection, &CalaosConnection::eventLogEventFinished,
               this, &EventLogItem::eventFinished);
    requestId.clear();

    update_loading(false);
    if (ok)
        load(data);
}
