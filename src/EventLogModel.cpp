#include "EventLogModel.h"
#include "RoomModel.h"

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

    connect(connection, &CalaosConnection::logEventLoaded,
            this, &EventLogModel::logEventLoaded);
}

QObject *EventLogModel::getEventModel(int idx) const
{
    EventLogItem *it = dynamic_cast<EventLogItem *>(item(idx));
    if (it) engine->setObjectOwnership(it, QQmlEngine::CppOwnership);
    return it;
}

void EventLogModel::load(int page, int per_page)
{
    if (loading) return;
    loading = true;

    QJsonObject d = {{ "page", page },
                     { "per_page", per_page }};
    connection->sendJson("eventlog", d);
}

void EventLogModel::loadMore()
{
    qDebug() << "Load more... rowCount:" << rowCount();
    if (loading) return;

    load(rowCount() / DEFAULT_PER_PAGE, DEFAULT_PER_PAGE);
}

void EventLogModel::logEventLoaded(const QVariantMap &data)
{
    if (!data.contains("events"))
        return;

    if (needClear)
        clear();
    needClear = false;

    QVariantList events = data["events"].toList();
    for (int i = 0;i < events.count();i++)
    {
        EventLogItem *it = new EventLogItem(engine, connection);
        it->load(events.at(i).toMap());

        appendRow(it);
    }

    loading = false;
}

QObject *EventLogModel::loadEvent(QString uuid)
{
    EventLogItem *it = new EventLogItem(engine, connection);
    it->load(uuid);
    return it;
}

void EventLogModel::refresh()
{
    qDebug() << "refresh...";
    if (loading) return;

    needClear = true;
    load();
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
    if (data["event_type"] == "3")
    {
        update_evType(Common::EventIoChanged);
        update_evTitle(tr("Appliance change"));
    }
    else if (data["event_type"] == "22")
    {
        update_evType(Common::EventPush);
        update_evTitle(tr("Push Notification"));
        update_evIconSource("icon_notif");

        QVariantMap e = data["event_raw"].toMap();
        update_evNotifText(e["message"].toString());

        if (e["pic_uid"].toString() == "")
        {
            update_evPictureUrl(QString());
            update_evHasPicture(false);
        }
        else
        {
            update_evPictureUrl(connection->getNotifPictureUrl(e["pic_uid"].toString()));
            update_evHasPicture(true);
        }
    }
    else
    {
        update_evType(Common::EventUnknown);
        update_evTitle(tr("Unknown event!"));
    }

    QDateTime dt = QDateTime::fromString(data["created_at"].toString(),
            "yyyy-MM-dd hh:mm:ss");
    dt = QDateTime(dt.date(), dt.time(), Qt::UTC).toLocalTime();

    if (dt.date() == QDate::currentDate())
        update_evDate(tr("Today"));
    else
        update_evDate(dt.date().toString(Qt::SystemLocaleShortDate));

    update_evTime(dt.time().toString("hh:mm:ss"));

    IOBase *io = IOCache::Instance().searchInput(data["io_id"].toString());
    if (!io)
        IOCache::Instance().searchOutput(data["io_id"].toString());
    if (io)
    {
        update_evTitle(io->get_ioName());
        update_evRoomName(io->get_room_name());

        if (io->get_ioType() == Common::Light ||
            io->get_ioType() == Common::Pump ||
            io->get_ioType() == Common::Outlet)
        {
            QString style = "light";
            if (io->get_ioType() == Common::Pump) style = "pump";
            if (io->get_ioType() == Common::Outlet) style = "outlet";

            if (data["io_state"].toString() == "true")
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
        else if (io->get_ioType() == Common::LightDimmer ||
                 io->get_ioType() == Common::LightRgb)
        {
            if (data["io_state"].toDouble() > 0)
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
        else if (io->get_ioType() == Common::Shutter ||
                 io->get_ioType() == Common::ShutterSmart)
        {
            if (data["io_state"].toString() == "true")
            {
                update_evIconSource("icon_shutter_on");
                update_evActionText(tr("Open"));
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
    update_loading(true);
    QJsonObject d = {{ "uuid", uuid }};
    connection->sendJson("eventlog", d);

    connect(connection, &CalaosConnection::logEventLoaded,
            this, &EventLogItem::logEventLoaded);
}

void EventLogItem::logEventLoaded(const QVariantMap &data)
{
    if (data.contains("events"))
        return;

    disconnect(connection, &CalaosConnection::logEventLoaded,
               this, &EventLogItem::logEventLoaded);

    update_loading(false);
    load(data);
}
