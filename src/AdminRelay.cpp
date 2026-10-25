#include "AdminRelay.h"
#include "CalaosConnection.h"
#include <QDateTime>
#include <QGuiApplication>
#include <QTimer>
#include <QDebug>

static const QString MsgUnlock = QStringLiteral("admin_unlock");
static const QString MsgAdmin = QStringLiteral("admin");

AdminRelay::AdminRelay(AdminTransport *transport, QObject *parent):
    QObject(parent),
    m_transport(transport)
{
    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setSingleShot(true);
    connect(m_sessionTimer, &QTimer::timeout, this, [this]()
    {
        forgetSession();
    });

    if (qGuiApp)
    {
        connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState st)
        {
            if (st == Qt::ApplicationActive)
                checkExpiry();
        });
    }
}

AdminRelay::~AdminRelay()
{
    //callbacks may reference objects already being destroyed
    for (Pending &p: m_pending)
        delete p.timer;
    m_pending.clear();
}

void AdminRelay::bindConnection(CalaosConnection *con)
{
    connect(con, &CalaosConnection::adminReplyReceived, this, &AdminRelay::handleReply);
    connect(con, &CalaosConnection::adminEventReceived, this, &AdminRelay::handleEvent);
    connect(con, &CalaosConnection::disconnected, this, &AdminRelay::handleTransportLost);
    connect(con, &CalaosConnection::loginFailed, this, &AdminRelay::handleTransportLost);
    connect(con, &CalaosConnection::tlsTrustAccepted, this, &AdminRelay::handleTlsTrustAccepted);
    connect(con, &CalaosConnection::connectionStateChanged, this, [this](int state)
    {
        if (state == ReconnectPolicy::Connected)
            handleTransportUp();
        else
            handleTransportLost();
    });
}

void AdminRelay::setTimeouts(int replyMs, int updateCheckMs)
{
    m_replyTimeoutMs = replyMs;
    m_updateCheckTimeoutMs = updateCheckMs;
}

int AdminRelay::expiresIn() const
{
    if (m_session.isEmpty())
        return 0;
    const qint64 mono = m_deadline.remainingTime() / 1000;
    const qint64 wall = (m_wallExpiry - QDateTime::currentMSecsSinceEpoch()) / 1000;
    return int(qMax<qint64>(0, qMin(mono, wall)));
}

void AdminRelay::unlock(const QString &password, Callback callback)
{
    //A key change waiting for an answer: the password must not reach a
    //server the user has not trusted yet.
    if (m_transport->hasPendingTlsTrust())
    {
        failLater(ErrTlsPending, callback);
        return;
    }

    send(MsgUnlock, QString(), QJsonObject{{ QStringLiteral("admin_pass"), password }},
         m_replyTimeoutMs, callback);
}

void AdminRelay::request(const QString &action, const QJsonObject &params, Callback callback)
{
    if (m_transport->hasPendingTlsTrust())
    {
        failLater(ErrTlsPending, callback);
        return;
    }

    QJsonObject data{{ QStringLiteral("action"), action }};
    //an empty key counts as present on the server side: leave it out
    if (!m_session.isEmpty())
        data[QStringLiteral("admin_session")] = m_session;
    if (!params.isEmpty())
        data[QStringLiteral("params")] = params;

    if (action == QLatin1String("update_status") && m_gate == GateIdle)
        setGate(GateProbing);

    send(MsgAdmin, action, data,
         action == QLatin1String("update_check") ? m_updateCheckTimeoutMs : m_replyTimeoutMs,
         callback);
}

QString AdminRelay::send(const QString &msg, const QString &action, const QJsonObject &data,
                         int timeoutMs, Callback callback)
{
    m_nextId++;
    const QString msgId = QStringLiteral("admin-%1").arg(m_nextId);

    if (!m_transport->sendAdminMessage(msg, data, msgId))
    {
        failLater(ErrWebsocketRequired, callback);
        return QString();
    }

    Pending p;
    p.msg = msg;
    p.action = action;
    p.callback = callback;
    p.timer = new QTimer(this);
    p.timer->setSingleShot(true);
    connect(p.timer, &QTimer::timeout, this, [this, msgId]()
    {
        Outcome o;
        o.error.code = ErrTimeout;
        finish(msgId, o);
    });
    p.timer->start(timeoutMs);
    m_pending.insert(msgId, p);
    return msgId;
}

void AdminRelay::failLater(const QString &code, Callback callback)
{
    //always asynchronous, like a reply: callers never re-enter themselves
    QMetaObject::invokeMethod(this, [this, code, callback]()
    {
        Outcome o;
        o.error.code = code;
        applyOutcome(QString(), QString(), o);
        if (callback)
            callback(o);
    }, Qt::QueuedConnection);
}

void AdminRelay::handleReply(const QString &msg, const QString &msgId, const QJsonObject &data)
{
    m_spoken = true;
    if (!m_pending.contains(msgId))
        return; //late answer to a request that timed out, or not ours

    Outcome o;
    if (data.value(QStringLiteral("success")).toString() == QLatin1String("true"))
    {
        o.ok = true;
        o.result = data.value(QStringLiteral("result"));
        if (msg == MsgUnlock)
        {
            const QString token = data.value(QStringLiteral("admin_session")).toString();
            if (token.isEmpty())
            {
                o.ok = false;
                o.error.code = ErrUnknown;
            }
            else
            {
                bool ok = false;
                int expires = data.value(QStringLiteral("expires_in")).toString().toInt(&ok);
                if (!ok || expires <= 0 || expires > MaxSessionSeconds)
                    expires = MaxSessionSeconds;
                setSession(token, expires);
            }
        }
    }
    else
    {
        o.error.code = data.value(QStringLiteral("error")).toString();
        if (o.error.code.isEmpty())
            o.error.code = ErrUnknown;
        o.error.msg = data.value(QStringLiteral("msg")).toString();
        bool ok = false;
        const int retry = data.value(QStringLiteral("retry_after")).toVariant().toInt(&ok);
        o.error.retryAfter = ok ? retry : -1;
        o.error.jobId = data.value(QStringLiteral("job_id")).toString();
    }

    finish(msgId, o);
}

void AdminRelay::finish(const QString &msgId, const Outcome &outcome)
{
    if (!m_pending.contains(msgId))
        return;
    Pending p = m_pending.take(msgId);
    if (p.timer)
    {
        p.timer->stop();
        p.timer->deleteLater();
    }

    Outcome o = outcome;
    applyOutcome(p.msg, p.action, o);
    if (o.error.code == ErrTimeout)
        qWarning() << "AdminRelay:" << (p.action.isEmpty() ? p.msg : p.action) << "got no answer";
    else if (!o.ok)
        qInfo() << "AdminRelay:" << (p.action.isEmpty() ? p.msg : p.action) << "refused:" << o.error.code;

    if (p.callback)
        p.callback(o);
}

void AdminRelay::applyOutcome(const QString &msg, const QString &action, Outcome &o)
{
    Q_UNUSED(action)

    if (o.ok)
    {
        if (msg == MsgUnlock)
            setGate(GateUnlocked);
        else if (m_gate != GateLocked && m_gate != GateUnlocked)
            setGate(isUnlocked() ? GateUnlocked : GateLocked);
        return;
    }

    const QString &code = o.error.code;
    if (code == QLatin1String("https_required"))
        setGate(GateHttpsRequired);
    else if (code == QLatin1String("admin_not_set"))
    {
        forgetSession();
        setGate(GateAdminNotSet);
    }
    else if (code == QLatin1String("admin_unavailable"))
        setGate(GateUnavailable);
    else if (code == QLatin1String("daemon_too_old"))
        setGate(GateTooOld);
    else if (code == QLatin1String("daemon_unreachable"))
        setGate(GateUnreachable);
    else if (code == QLatin1String("bad_session"))
    {
        //expired or invalidated: ask again, silently
        forgetSession();
        setGate(GateLocked);
    }
    else if (code == ErrTimeout)
    {
        //an old calaos_server ignores unknown messages in silence; silence
        //later on, or while dpkg keeps the box busy, is a stall
        if (!m_spoken && !m_upgrading)
            setGate(GateOldServer);
        else if (m_gate != GateUnlocked && m_gate != GateLocked)
            setGate(GateUnreachable);
    }
    else if (code == ErrDisconnected || code == ErrWebsocketRequired || code == ErrTlsPending)
    {
        if (m_gate == GateProbing)
            setGate(GateIdle);
    }
    else if (m_gate == GateProbing)
    {
        //internal and friends on the probe: nothing to type, wait and retry
        setGate(GateUnreachable);
    }
}

void AdminRelay::handleEvent(const QJsonObject &data)
{
    const QString type = data.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("upgrade_progress"))
    {
        emit upgradeProgress(data.value(QStringLiteral("data")).toObject());
    }
    else if (type == QLatin1String("admin_set") || type == QLatin1String("admin_reset"))
    {
        //every session of the box is invalidated, ours included
        const bool had = isUnlocked();
        forgetSession();
        if (had)
            emit sessionInvalidated();
        if (type == QLatin1String("admin_reset"))
        {
            setGate(GateIdle);
            emit adminReset();
        }
    }
}

void AdminRelay::handleTransportLost()
{
    const QHash<QString, Pending> pending = m_pending;
    m_pending.clear();
    m_spoken = false;
    forgetSession();
    if (m_gate == GateProbing)
        setGate(GateIdle);

    for (auto it = pending.begin(); it != pending.end(); ++it)
    {
        Pending p = it.value();
        if (p.timer)
        {
            p.timer->stop();
            p.timer->deleteLater();
        }
        Outcome o;
        o.error.code = ErrDisconnected;
        if (p.callback)
            p.callback(o);
    }
}

void AdminRelay::handleTransportUp()
{
    emit transportUp();
}

void AdminRelay::handleTlsTrustAccepted()
{
    forgetSession();
}

void AdminRelay::checkExpiry()
{
    if (isUnlocked() && expiresIn() <= 0)
        forgetSession();
}

void AdminRelay::lock()
{
    forgetSession();
}

void AdminRelay::setGate(const QString &gate)
{
    if (m_gate == gate)
        return;
    m_gate = gate;
    emit gateChanged();
}

void AdminRelay::setSession(const QString &session, int expiresInSeconds)
{
    m_session = session;
    m_deadline = QDeadlineTimer(qint64(expiresInSeconds) * 1000);
    m_wallExpiry = QDateTime::currentMSecsSinceEpoch() + qint64(expiresInSeconds) * 1000;
    //the lifetime is absolute: lock a second early rather than have the next
    //tap answered bad_session
    m_sessionTimer->start(qMax(0, expiresInSeconds - 1) * 1000);
    emit sessionChanged();
}

void AdminRelay::forgetSession()
{
    m_sessionTimer->stop();
    if (m_session.isEmpty())
        return;
    m_session.clear();
    m_wallExpiry = 0;
    m_deadline = QDeadlineTimer();
    if (m_gate == GateUnlocked)
        setGate(GateLocked);
    emit sessionChanged();
}
