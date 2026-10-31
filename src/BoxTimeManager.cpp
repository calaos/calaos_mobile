#include "BoxTimeManager.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QPointer>
#include <QTimeZone>
#include "BoxTime.h"
#include "CalaosOsAPI.h"

using namespace BoxTime;

//same threshold as the web interface
static const qint64 STEP_CONFIRM_MS = 60 * 60 * 1000;

static const QString ZONE_KEY = QStringLiteral("timezone");
static const QString NTP_KEY = QStringLiteral("ntp");
static const QString TIME_KEY = QStringLiteral("time");

BoxTimeManager::BoxTimeManager(CalaosOsAPI *api, QObject *parent):
    QObject(parent),
    m_api(api),
    m_locale(QLocale::system())
{
    m_loaded = false;
    m_updateRequired = false;
    m_busy = false;
    m_ntp = false;
    m_restartRequired = false;
    m_serverRestartDone = false;
    m_messageIsError = false;
    m_unconfirmed = false;
    m_zonesLoading = false;

    m_ticker.setInterval(1000);
    connect(&m_ticker, &QTimer::timeout, this, &BoxTimeManager::updateClock);
}

void BoxTimeManager::setLanguage(const QString &code)
{
    m_locale = code.isEmpty() ? QLocale::system() : QLocale(code);
    updateClock();
}

void BoxTimeManager::setZoneFilter(const QString &filter)
{
    if (filter == m_zoneFilter)
        return;
    m_zoneFilter = filter;
    emit zoneFilterChanged(filter);
    updateZones();
}

void BoxTimeManager::activate()
{
    m_ticker.start();
    setZoneFilter(QString());

    //a write still running reads the box when it ends, and has things to say
    if (!m_writing)
    {
        setMessage(QString(), false);
        update_unconfirmed(false);
        clearState();
        read();
    }

    if (m_allZones.isEmpty())
        loadZones();
}

void BoxTimeManager::deactivate()
{
    m_ticker.stop();
}

void BoxTimeManager::refresh()
{
    if (get_busy())
        return;
    read();
}

void BoxTimeManager::read()
{
    const quint64 id = ++m_readId;
    QPointer<BoxTimeManager> self(this);
    m_api->getBoxTime([self, id](const CalaosOsReply &reply)
    {
        if (!self || id != self->m_readId)
            return;

        if (!reply.ok || !self->applyState(reply.data.toObject()))
            self->readFailed(reply);

        if (self->m_writing)
        {
            self->m_writing = false;
            self->update_busy(false);
        }
    });
}

void BoxTimeManager::readFailed(const CalaosOsReply &reply)
{
    clearState();
    if (reply.httpStatus == 404)
    {
        update_updateRequired(true);
        return;
    }

    qWarning() << "system/time read failed:" << reply.httpStatus << reply.error;
    update_loadError(tr("The date and time of the box could not be read."));
}

bool BoxTimeManager::applyState(const QJsonObject &state)
{
    const QJsonValue zone = state.value(ZONE_KEY);
    const QJsonValue ntp = state.value(NTP_KEY);
    const QDateTime now = QDateTime::fromString(state.value(QStringLiteral("now")).toString(), Qt::ISODateWithMs);
    if (!now.isValid() || !zone.isString() || !ntp.isBool())
        return false;

    m_readAtMs = now.toMSecsSinceEpoch();
    m_sinceRead.start();

    update_timezone(zone.toString());
    update_ntp(ntp.toBool());
    if (!ntp.toBool())
        update_ntpState(QStringLiteral("manual"));
    else if (state.value(QStringLiteral("synchronized")).toBool())
        update_ntpState(QStringLiteral("synchronized"));
    else
        update_ntpState(QStringLiteral("notSynchronized"));

    update_updateRequired(false);
    update_loadError(QString());
    update_loaded(true);
    updateClock();
    updateZones();

    if (!m_unconfirmedZone.isEmpty() && get_timezone() == m_unconfirmedZone &&
        m_unconfirmedZone != m_zoneBeforeWrite)
    {
        m_unconfirmedZone.clear();
        update_serverRestartDone(false);
        update_restartRequired(true);
    }

    return true;
}

void BoxTimeManager::clearState()
{
    update_loaded(false);
    update_updateRequired(false);
    update_loadError(QString());
    update_dateText(QString());
    update_timeText(QString());
    update_timezone(QString());
    update_ntp(false);
    update_ntpState(QString());
    updateZones();
}

void BoxTimeManager::loadZones()
{
    if (get_zonesLoading())
        return;

    update_zonesLoading(true);
    update_zonesError(QString());

    QPointer<BoxTimeManager> self(this);
    m_api->getTimezones([self](const CalaosOsReply &reply)
    {
        if (!self)
            return;

        self->update_zonesLoading(false);

        QStringList zones;
        const QJsonArray list = reply.data.toObject().value(QStringLiteral("timezones")).toArray();
        for (const QJsonValue &zone: list)
        {
            if (zone.isString())
                zones << zone.toString();
        }

        if (!reply.ok || zones.isEmpty())
        {
            qWarning() << "system/timezones failed:" << reply.httpStatus << reply.error;
            self->update_zonesError(tr("The list of time zones could not be read."));
            return;
        }

        self->m_allZones = zones;
        self->updateZones();
    });
}

void BoxTimeManager::updateZones()
{
    QStringList all = m_allZones;
    if (!get_timezone().isEmpty() && !all.isEmpty() && !all.contains(get_timezone()))
        all.prepend(get_timezone());
    update_zones(filterZones(all, m_zoneFilter));
}

void BoxTimeManager::setTimezone(const QString &zone)
{
    if (zone.isEmpty() || zone == get_timezone())
        return;
    write({{ ZONE_KEY, zone }});
}

void BoxTimeManager::setNtp(bool enabled)
{
    write({{ NTP_KEY, enabled }});
}

void BoxTimeManager::applyManualTime(const QString &time)
{
    QJsonObject change{{ TIME_KEY, time }};
    //the daemon refuses a time while NTP is on
    if (get_ntp())
        change[NTP_KEY] = false;
    write(change);
}

void BoxTimeManager::write(const QJsonObject &change)
{
    if (get_busy() || !get_loaded())
        return;

    m_writing = true;
    update_busy(true);
    setMessage(QString(), false);
    update_unconfirmed(false);
    m_unconfirmedZone.clear();

    //a read already on its way may predate this write
    m_readId++;

    const QString zoneBefore = get_timezone();
    QPointer<BoxTimeManager> self(this);
    m_api->setBoxTime(change, [self, change, zoneBefore](const CalaosOsReply &reply)
    {
        if (self)
            self->writeAnswered(change, zoneBefore, reply);
    });
}

void BoxTimeManager::writeAnswered(const QJsonObject &change, const QString &zoneBefore, const CalaosOsReply &reply)
{
    const QJsonObject data = reply.data.toObject();
    bool zoneChanged = false;

    if (reply.ok)
        zoneChanged = data.value(QStringLiteral("restart_required")).toBool();
    else
    {
        //the raw daemon text is English and transport errors are Qt internals
        qWarning() << "system/time write failed:" << reply.httpStatus << reply.error;

        const QJsonValue applied = data.value(QStringLiteral("applied"));
        if (reply.httpStatus == 404)
            update_updateRequired(true);
        else if (reply.upgradeInProgress)
            setMessage(tr("An update is being installed. Change the time once it has finished."), true);
        else if (reply.httpStatus == 400)
            setMessage(refusedText(change), true);
        else if (reply.neverReachedDaemon())
            setMessage(tr("The box could not be reached. Nothing was changed."), true);
        else if (reply.httpStatus == 0)
        {
            setMessage(tr("The change may have been made, the box did not answer. Check the values above before applying it again."), true);
            update_unconfirmed(true);
            if (change.contains(ZONE_KEY))
            {
                m_unconfirmedZone = change.value(ZONE_KEY).toString();
                m_zoneBeforeWrite = zoneBefore;
            }
        }
        else if (applied.isArray())
        {
            setMessage(partialText(data), true);
            zoneChanged = applied.toArray().contains(ZONE_KEY);
        }
        else
            setMessage(tr("The box could not apply this change."), true);
    }

    if (zoneChanged)
    {
        update_serverRestartDone(false);
        update_restartRequired(true);
    }

    //busy until this read answers: its values are what the next write is judged on
    read();
}

static QString stepName(const QString &step)
{
    if (step == ZONE_KEY)
        return BoxTimeManager::tr("time zone");
    if (step == NTP_KEY)
        return BoxTimeManager::tr("automatic time");
    return BoxTimeManager::tr("time");
}

QString BoxTimeManager::partialText(const QJsonObject &data) const
{
    QStringList applied;
    const QJsonArray steps = data.value(QStringLiteral("applied")).toArray();
    for (const QJsonValue &step: steps)
        applied << stepName(step.toString());

    const QString failed = data.value(QStringLiteral("failed")).toString();
    if (failed.isEmpty())
        return tr("The change was made, but the box could not read it back.");
    if (applied.isEmpty())
        return tr("Not changed: %1.").arg(stepName(failed));
    return tr("Changed: %1. Not changed: %2.").arg(applied.join(QStringLiteral(", ")), stepName(failed));
}

QString BoxTimeManager::refusedText(const QJsonObject &change) const
{
    if (change.contains(TIME_KEY))
        return tr("The box refused this date and time.");
    if (change.contains(ZONE_KEY))
        return tr("The box does not know this time zone.");
    return tr("The box refused this change.");
}

QString BoxTimeManager::clockZone() const
{
    return get_timezone().isEmpty() ? QStringLiteral("UTC") : get_timezone();
}

qint64 BoxTimeManager::boxNowMs() const
{
    return m_readAtMs + m_sinceRead.elapsed();
}

QDateTime BoxTimeManager::boxNow() const
{
    const QTimeZone zone(clockZone().toUtf8());
    return QDateTime::fromMSecsSinceEpoch(boxNowMs(), zone.isValid() ? zone : QTimeZone(QTimeZone::UTC));
}

void BoxTimeManager::updateClock()
{
    if (!get_loaded())
        return;

    const QDateTime now = boxNow();
    update_dateText(m_locale.toString(now.date(), QLocale::LongFormat));
    //no zone, or one this machine does not know: UTC, and say so
    const bool utc = get_timezone().isEmpty() || !QTimeZone(get_timezone().toUtf8()).isValid();
    update_timeText(now.toString(QStringLiteral("HH:mm:ss")) + (utc ? QStringLiteral(" UTC") : QString()));
}

QVariantMap BoxTimeManager::localNow() const
{
    const QDateTime now = boxNow();
    return {
        { "year", now.date().year() }, { "month", now.date().month() }, { "day", now.date().day() },
        { "hour", now.time().hour() }, { "minute", now.time().minute() }, { "second", now.time().second() },
    };
}

static QString amountText(qint64 ms)
{
    const qint64 seconds = qAbs(ms) / 1000;
    //rounded first: the clock has run since the read, a two hour step is 1 h 59
    const qint64 minutes = qRound64(seconds / 60.0);
    if (minutes < 120)
        return BoxTimeManager::tr("%1 min").arg(minutes);
    const qint64 hours = qRound64(seconds / 3600.0);
    if (hours < 48)
        return BoxTimeManager::tr("%1 h").arg(hours);
    return BoxTimeManager::tr("%1 days").arg(qRound64(seconds / 86400.0));
}

QVariantMap BoxTimeManager::checkManualTime(int year, int month, int day, int hour, int minute, int second) const
{
    //the offset of the step and the state of NTP would be guesses
    if (!get_loaded())
        return {{ "ok", false }, { "problem", tr("The date and time of the box could not be read.") }};

    const LocalTime local = fromLocal(year, month, day, hour, minute, second, clockZone());

    QString problem;
    switch (local.status)
    {
    case LocalStatus::Ok:
        break;
    case LocalStatus::Invalid:
        problem = tr("Enter a complete date and time.");
        break;
    case LocalStatus::Range:
        problem = tr("The year must be between %1 and %2.").arg(FIRST_YEAR).arg(LAST_YEAR);
        break;
    case LocalStatus::Nonexistent:
        problem = tr("This time does not exist in %1: clocks skip it at the change to summer time.").arg(clockZone());
        break;
    case LocalStatus::UnknownZone:
        problem = tr("This screen does not know the time zone %1: the time cannot be set from here.").arg(clockZone());
        break;
    }

    if (!problem.isEmpty())
        return {{ "ok", false }, { "problem", problem }};

    const qint64 step = local.ms - boxNowMs();
    const bool bigStep = qAbs(step) > STEP_CONFIRM_MS;

    QStringList confirm;
    confirm << tr("The box clock will be set to %1, %2 (%3).")
               .arg(m_locale.toString(QDate(year, month, day), QLocale::LongFormat),
                    QTime(hour, minute, second).toString(QStringLiteral("HH:mm:ss")),
                    clockZone());
    if (get_ntp())
        confirm << tr("Automatic time (NTP) will be turned off: the box stops synchronizing with time servers.");
    if (bigStep)
        confirm << tr("The box clock will move by %1. Moving it back can break secure connections and updates until the time is right.")
                   .arg(amountText(step));

    return {
        { "ok", true },
        { "time", local.rfc3339 },
        { "needsConfirm", get_ntp() || bigStep },
        { "confirmText", confirm.join(QStringLiteral("\n\n")) },
    };
}

void BoxTimeManager::restartServer()
{
    if (get_busy())
        return;

    update_busy(true);
    setMessage(QString(), false);

    QPointer<BoxTimeManager> self(this);
    m_api->restartService(QStringLiteral("calaos-server"), [self](const CalaosOsReply &reply)
    {
        if (!self)
            return;

        self->update_busy(false);

        if (reply.ok)
        {
            self->update_serverRestartDone(true);
            self->setMessage(tr("The Calaos server is restarting."), false);
            return;
        }

        qWarning() << "services/restart refused:" << reply.httpStatus << reply.error;
        if (reply.httpStatus == 404)
            self->setMessage(tr("This box must be updated to restart the Calaos server from here."), true);
        else if (reply.upgradeInProgress)
            self->setMessage(tr("An update is being installed. Restart once it has finished."), true);
        else
            self->setMessage(tr("The Calaos server could not be restarted."), true);
    });
}

void BoxTimeManager::dismissRestart()
{
    update_restartRequired(false);
}

void BoxTimeManager::setMessage(const QString &text, bool isError)
{
    update_message(text);
    update_messageIsError(isError);
}
