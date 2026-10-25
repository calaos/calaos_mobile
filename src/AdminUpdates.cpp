#include "AdminUpdates.h"
#include "UpdateGrouping.h"
#include <algorithm>
#include <QDateTime>
#include <QJsonArray>
#include <QTimer>
#include <QtMath>

using namespace CalaosOsProtocol;

//webapp: only update_status may be polled while an upgrade runs
static const int JobPollMs = 5000;
//webapp: admin_unavailable is retried fast during the first 2 minutes
static const qint64 UnavailableFastWindowMs = 120000;
//webapp: daemon_too_old that lasts longer means the box needs a reboot
static const int RebootHintMs = 120000;
//webapp: a finished job older than this is history, not news
static const qint64 JobNewsWindowSecs = 24 * 3600;

AdminUpdates::AdminUpdates(UpdateBackend *backend, AdminRelay *relay, QObject *parent):
    QObject(parent),
    m_backend(backend),
    m_relay(relay)
{
    m_packages = new UpdatePackagesModel(this);
    m_groups = new UpdateGroupsModel(this);
    m_progressRows = new UpdateProgressModel(this);

    update_gate(m_relay->gate());
    update_isUnlocked(m_relay->isUnlocked());
    update_busy(QString());
    update_phase(QStringLiteral("unknown"));
    update_updatesCount(0);
    update_totalPercent(0);
    update_currentPackage(QString());
    update_pkgIndex(0);
    update_pkgCount(0);
    update_externalRun(false);
    update_failureText(QString());
    update_errorText(QString());
    update_errorCode(QString());
    update_retryAfter(0);
    update_info(QString());
    update_rebootHint(false);
    update_boxName(QString());

    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(true);
    m_pollTimer->setInterval(JobPollMs);
    connect(m_pollTimer, &QTimer::timeout, this, &AdminUpdates::refreshStatus);

    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &AdminUpdates::probe);

    m_rebootHintTimer = new QTimer(this);
    m_rebootHintTimer->setSingleShot(true);
    m_rebootHintTimer->setInterval(RebootHintMs);
    connect(m_rebootHintTimer, &QTimer::timeout, this, [this]()
    {
        if (get_gate() == AdminRelay::GateTooOld)
            update_rebootHint(true);
    });

    m_heldTimer = new QTimer(this);
    m_heldTimer->setSingleShot(true);
    m_heldTimer->setInterval(HeldUnlockMs);
    connect(m_heldTimer, &QTimer::timeout, this, &AdminUpdates::dropHeldUnlock);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]()
    {
        const int left = get_retryAfter() - 1;
        update_retryAfter(qMax(0, left));
        if (left <= 0)
        {
            m_countdownTimer->stop();
            if (get_errorCode() == QLatin1String("rate_limited"))
                dismissError();
        }
        else if (get_errorCode() == QLatin1String("rate_limited"))
            update_errorText(errorTextFor(get_errorCode(), QString(), left));
    });

    m_backend->setProgressHandler([this](const QJsonObject &status) { onProgressEvent(status); });

    connect(m_relay, &AdminRelay::gateChanged, this, &AdminUpdates::onGateChanged);
    connect(m_relay, &AdminRelay::sessionChanged, this, [this]()
    {
        const bool unlocked = m_relay->isUnlocked();
        update_isUnlocked(unlocked);
        //the household view carries no package names: none may outlive the session
        if (!unlocked)
            forgetBoxData();
    });
    connect(m_relay, &AdminRelay::sessionInvalidated, this, [this]()
    {
        update_info(QStringLiteral("session_invalidated"));
    });
    connect(m_relay, &AdminRelay::adminReset, this, [this]()
    {
        if (m_active)
            probe();
    });
    connect(m_relay, &AdminRelay::transportUp, this, [this]()
    {
        //update_status resumes the display of a run that outlived calaos_server
        if (m_active || isUpgrading())
            probe();
    });
}

void AdminUpdates::setPollInterval(int ms)
{
    m_pollTimer->setInterval(ms);
}

void AdminUpdates::setRetryDelays(int fastMs, int slowMs)
{
    m_fastRetryMs = fastMs;
    m_slowRetryMs = slowMs;
}

bool AdminUpdates::isUpgrading() const
{
    return (m_job.valid && m_job.status == QLatin1String("running")) ||
           m_progress.value(QStringLiteral("status")).toString() == QLatin1String("upgrading");
}

void AdminUpdates::activate()
{
    m_active = true;
    probe();
}

void AdminUpdates::holdUnlock(const QString &password)
{
    m_heldPassword = password;
    m_heldHost = m_relay->hostKey();
    m_heldTimer->start();
}

void AdminUpdates::dropHeldUnlock()
{
    m_heldTimer->stop();
    m_heldPassword.clear();
    m_heldHost.clear();
}

void AdminUpdates::sendHeldUnlock()
{
    if (m_heldPassword.isEmpty())
        return;
    const QString password = m_heldPassword;
    const bool sameBox = m_heldHost == m_relay->hostKey();
    dropHeldUnlock();
    //never to another box than the one it was typed for; AdminRelay still
    //refuses anything but a proven wss socket without a certificate question
    if (sameBox && m_relay->gate() == AdminRelay::GateLocked)
        unlock(password);
}

void AdminUpdates::deactivate()
{
    dropHeldUnlock();
    m_active = false;
    m_retryTimer->stop();
    m_unavailableSince = 0;
    update_info(QString());
    if (get_errorCode() != QLatin1String("rate_limited"))
        dismissError();
}

void AdminUpdates::retry()
{
    dismissError();
    probe();
}

void AdminUpdates::probe()
{
    m_retryTimer->stop();
    refreshStatus();
}

void AdminUpdates::refreshStatus()
{
    m_pollTimer->stop();
    m_statusReads++;
    m_backend->getUpdateStatus([this](bool ok, const UpdateStatus &status)
    {
        m_statusReads--;
        if (ok)
            applyStatus(status);
        else if (isUpgrading())
            m_pollTimer->start(); //a transient failure must not end the follow-up

        //the run ended while this read was out: it may predate the end
        if (m_rereadAfter && m_statusReads == 0)
        {
            m_rereadAfter = false;
            refreshStatus();
        }
    });
}

void AdminUpdates::listAvailable()
{
    m_backend->getAvailableUpdates([this](bool ok, const QJsonObject &pkgs, const QString &)
    {
        //an empty list may only mean that no check ran yet
        if (ok && !pkgs.isEmpty() && m_relay->isUnlocked() && !isUpgrading())
        {
            applyAvailable(pkgs);
            recompute();
        }
    });
}

void AdminUpdates::readSystemInfo()
{
    m_backend->getSystemInfo([this](bool ok, const QJsonValue &res)
    {
        if (ok && m_relay->isUnlocked())
            update_boxName(res.toObject().value(QStringLiteral("hostname")).toString());
    });
}

void AdminUpdates::setHeldUnlockTimeout(int ms)
{
    m_heldTimer->setInterval(ms);
}

void AdminUpdates::unlock(const QString &password)
{
    if (password.isEmpty())
        return;
    //the socket is being reopened: the answer waits for the probe
    const QString gate = m_relay->gate();
    if (m_active && (gate == AdminRelay::GateIdle || gate == AdminRelay::GateProbing))
    {
        holdUnlock(password);
        return;
    }
    if (!get_busy().isEmpty() || get_retryAfter() > 0)
        return;

    update_busy(QStringLiteral("unlock"));
    dismissError();
    update_info(QString());
    m_relay->unlock(password, [this](const AdminRelay::Outcome &o)
    {
        update_busy(QString());
        if (o.ok)
            return; //onGateChanged reads the full view
        if (o.error.code == QLatin1String("rate_limited"))
            startCountdown(o.error.retryAfter > 0 ? o.error.retryAfter : 60);
        showError(o.error.code, o.error.msg);
    });
}

void AdminUpdates::checkUpdates()
{
    if (!get_busy().isEmpty() || isUpgrading())
        return;

    update_busy(QStringLiteral("update_check"));
    dismissError();
    m_backend->checkUpdates([this](const CalaosOsReply &r)
    {
        update_busy(QString());
        if (r.ok)
        {
            //the session may have ended during the check (125 s)
            if (m_relay->isUnlocked())
            {
                applyAvailable(r.data.toObject());
                recompute();
            }
            return;
        }
        showError(r);
        //a run started elsewhere (kiosk, webapp): follow it instead
        if (r.upgradeInProgress)
            refreshStatus();
    });
}

void AdminUpdates::upgradeAll()
{
    if (!get_busy().isEmpty() || isUpgrading() || get_updatesCount() == 0)
        return;

    update_busy(QStringLiteral("update_upgrade_all"));
    dismissError();
    m_backend->upgradeAll([this](const CalaosOsReply &r)
    {
        update_busy(QString());
        if (!r.ok)
        {
            showError(r);
            //upgrade_in_progress: show that run. No answer at all: the run
            //may have started, update_status tells (the reconnect reads it
            //again if the socket is gone)
            const bool unanswered = r.code == AdminRelay::ErrTimeout ||
                                    r.code == AdminRelay::ErrDisconnected ||
                                    r.code == AdminRelay::ErrSessionLost;
            if (r.upgradeInProgress || unanswered)
                refreshStatus();
            return;
        }

        const Job started = parseJob(r.data);
        if (started.valid)
        {
            m_job = started;
            m_ownJobId = started.id;
            m_seenRunning.insert(started.id);
        }
        m_progress = QJsonObject{{ QStringLiteral("status"), QStringLiteral("upgrading") },
                                 { QStringLiteral("total_percent"), 0 },
                                 { QStringLiteral("pkg_count"), get_updatesCount() }};
        m_packageNames.clear();
        m_namesAskedFor = 0;
        m_updatesKnown = false;
        m_relay->setUpgrading(true);
        m_pollTimer->start();
        recompute();
    });
}

void AdminUpdates::reboot()
{
    if (!get_busy().isEmpty() || isUpgrading())
        return;

    update_busy(QStringLiteral("system_reboot"));
    dismissError();
    m_backend->rebootMachine([this](const CalaosOsReply &r)
    {
        update_busy(QString());
        if (r.ok)
        {
            update_info(QStringLiteral("reboot_sent"));
            return;
        }
        showError(r);
        //a run started elsewhere (kiosk, webapp): follow it instead
        if (r.upgradeInProgress)
            refreshStatus();
    });
}

void AdminUpdates::dismissError()
{
    update_errorText(QString());
    update_errorCode(QString());
}

void AdminUpdates::dismissInfo()
{
    update_info(QString());
}

void AdminUpdates::startCountdown(int seconds)
{
    update_retryAfter(seconds);
    m_countdownTimer->start();
}

bool AdminUpdates::isGateCode(const QString &code)
{
    static const QSet<QString> codes = {
        QStringLiteral("https_required"), QStringLiteral("admin_not_set"),
        QStringLiteral("admin_unavailable"), QStringLiteral("daemon_too_old"),
        QStringLiteral("daemon_unreachable"), QStringLiteral("bad_session"),
        AdminRelay::ErrWebsocketRequired, AdminRelay::ErrSessionLost,
    };
    return codes.contains(code);
}

void AdminUpdates::showError(const CalaosOsReply &r)
{
    if (r.code == QLatin1String("rate_limited"))
        startCountdown(r.retryAfter > 0 ? r.retryAfter : 60);
    showError(r.code, r.error);
}

void AdminUpdates::showError(const QString &code, const QString &msg)
{
    if (isGateCode(code))
        return;
    //silence from an old calaos_server is the old_server screen
    if (code == AdminRelay::ErrTimeout && get_gate() == AdminRelay::GateOldServer)
        return;
    update_errorCode(code);
    update_errorText(errorTextFor(code, msg, get_retryAfter()));
}

QString AdminUpdates::errorTextFor(const QString &code, const QString &msg, int retryAfter)
{
    const QString detail = msg.isEmpty() ? QString() : QStringLiteral(" ") + msg;
    if (code == QLatin1String("bad_password"))
        return tr("Wrong admin password.");
    if (code == QLatin1String("rate_limited"))
        return tr("Too many attempts. Try again in %1 s.").arg(qMax(1, retryAfter));
    if (code == QLatin1String("upgrade_in_progress"))
        return tr("An update is already in progress.");
    if (code == QLatin1String("not_allowed"))
        return tr("This action is not allowed.") + detail;
    if (code == QLatin1String("invalid_request"))
        return tr("The box refused the request.") + detail;
    if (code == QLatin1String("package_not_found"))
        return tr("This package has no update waiting.") + detail;
    if (code == QLatin1String("package_held"))
        return tr("This package is held on the box and won't be updated.");
    if (code == QLatin1String("internal"))
        return tr("Something went wrong on the box. Try again; the detail is in the box's logs.");
    if (code == AdminRelay::ErrTimeout)
        return tr("The box did not answer. Try again in a moment.");
    if (code == AdminRelay::ErrDisconnected)
        return tr("Connection lost before the box answered. Try again once it is back.");
    if (code == AdminRelay::ErrNotReady)
        return tr("The box has not answered yet. Try again in a moment.");
    if (code == AdminRelay::ErrTlsPending)
        return tr("Answer the certificate question first.");
    return tr("Unexpected answer from the box.") + detail;
}

void AdminUpdates::onGateChanged()
{
    const QString gate = m_relay->gate();
    const QString previous = get_gate();
    update_gate(gate);

    //an error of the previous socket says nothing about the next one; a
    //rate limit still holds whatever the socket
    if (gate == AdminRelay::GateIdle && get_errorCode() != QLatin1String("rate_limited"))
        dismissError();

    if (!m_heldPassword.isEmpty())
    {
        //sent once the box answered on the new socket, outside the reply
        //being handled; any other screen than waiting or locked drops it
        if (gate == AdminRelay::GateLocked)
            QMetaObject::invokeMethod(this, &AdminUpdates::sendHeldUnlock, Qt::QueuedConnection);
        else if (gate != AdminRelay::GateIdle && gate != AdminRelay::GateProbing)
            dropHeldUnlock();
    }

    const bool blocked = gate == AdminRelay::GateUnavailable ||
                         gate == AdminRelay::GateTooOld ||
                         gate == AdminRelay::GateUnreachable;
    if (blocked)
    {
        if (gate != previous)
            m_retryAttempt = 0;
        if (gate == AdminRelay::GateTooOld && !m_rebootHintTimer->isActive() && !get_rebootHint())
            m_rebootHintTimer->start();
        scheduleRetry();
    }
    else
    {
        m_retryTimer->stop();
        m_unavailableSince = 0;
        m_retryAttempt = 0;
        m_rebootHintTimer->stop();
        update_rebootHint(false);
    }

    if (gate == AdminRelay::GateUnlocked && previous != AdminRelay::GateUnlocked)
    {
        //the full view: package names, the job's own error
        refreshStatus();
        listAvailable();
        readSystemInfo();
    }
}

void AdminUpdates::scheduleRetry()
{
    m_retryTimer->stop();
    if (!m_active)
        return;

    const QString gate = get_gate();
    int delay = 0;
    if (gate == AdminRelay::GateUnavailable)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_unavailableSince == 0)
            m_unavailableSince = now;
        delay = now - m_unavailableSince < UnavailableFastWindowMs ? m_fastRetryMs : m_slowRetryMs;
    }
    else
    {
        m_unavailableSince = 0;
        delay = int(qMin<qreal>(m_fastRetryMs * qPow(1.5, m_retryAttempt), m_fastRetryMs * 6.0));
        m_retryAttempt++;
    }
    m_retryTimer->start(delay);
}

//Twin of the idle/terminal frame handling of UpdateManager::applyStatusSnapshot() (kiosk).
QJsonObject AdminUpdates::held(const QJsonObject &next) const
{
    //while a run is followed, its last frames (done, then idle at 0 %) must
    //not rewind the bar before the job's end is read back
    if (m_progress.isEmpty() || !isUpgrading())
        return next;
    if (isIdleProgress(next))
        return m_progress;
    QJsonObject out = next;
    if (next.value(QStringLiteral("total_percent")).toDouble() <
        m_progress.value(QStringLiteral("total_percent")).toDouble())
        out[QStringLiteral("total_percent")] = m_progress.value(QStringLiteral("total_percent"));
    return out;
}

void AdminUpdates::applyStatus(const UpdateStatus &status)
{
    const Job next = status.hasJobField ? status.job : Job();
    if (next.id != m_job.id)
    {
        m_packageNames.clear();
        m_namesAskedFor = 0;
        //a run this screen did not see start makes the last check stale
        if (next.valid)
            m_updatesKnown = false;
    }

    const bool running = next.valid && next.status == QLatin1String("running");
    m_progress = running ? held(status.progress) : status.progress;
    m_job = next;

    if (running)
    {
        m_seenRunning.insert(next.id);
        m_relay->setUpgrading(true);
        m_pollTimer->start();
    }
    else
    {
        m_pollTimer->stop();
        m_relay->setUpgrading(isUpgrading());
        //the run ended: what it left to install is read again
        if (next.valid && m_seenRunning.contains(next.id) && m_relay->isUnlocked())
            listAvailable();
    }

    applyProgress(m_progress);
    recompute();
}

//Twin of the package part of UpdateManager::applyStatusSnapshot() (kiosk).
void AdminUpdates::applyProgress(const QJsonObject &progress)
{
    const QJsonArray packages = progress.value(QStringLiteral("packages")).toArray();
    QStringList names;
    for (const QJsonValue &v: packages)
        names.append(v.toObject().value(QStringLiteral("name")).toString());

    //only the full view (admin session) carries names
    const bool named = m_relay->isUnlocked() &&
                       std::any_of(names.cbegin(), names.cend(), [](const QString &n) { return !n.isEmpty(); });
    if (!named)
        return;

    m_packageNames = names;
    m_progressRows->syncPackages(packages);
    for (const QJsonValue &v: packages)
    {
        const QJsonObject p = v.toObject();
        const QString name = p.value(QStringLiteral("name")).toString();
        if (UpdateGrouping::groupForPackage(name).isEmpty())
            m_packages->setPkgProgress(name, p.value(QStringLiteral("state")).toString(),
                                       p.value(QStringLiteral("percent")).toInt());
    }
}

//Twin of the terminal frame handling of UpdateManager::applyStatusSnapshot() (kiosk).
void AdminUpdates::onProgressEvent(const QJsonObject &status)
{
    const bool following = isUpgrading();
    m_progress = held(status);
    const QString st = status.value(QStringLiteral("status")).toString();

    if (st == QLatin1String("done") || st == QLatin1String("error") ||
        (following && isIdleProgress(status)))
    {
        //the job's reboot_required and error only come from update_status
        m_pollTimer->stop();
        if (m_statusReads > 0)
            m_rereadAfter = true;
        else
            refreshStatus();
    }
    else if (st == QLatin1String("upgrading") && m_statusReads == 0)
    {
        const int index = status.value(QStringLiteral("pkg_index")).toInt();
        //a run started elsewhere: learn its job; a new package: its name
        if (!m_job.valid || m_job.status != QLatin1String("running"))
            refreshStatus();
        else if (m_relay->isUnlocked() && index > m_packageNames.size() && index != m_namesAskedFor)
        {
            m_namesAskedFor = index;
            refreshStatus();
        }
    }

    m_relay->setUpgrading(isUpgrading());
    recompute();
}

//Twin of UpdateManager::applyAvailable() (kiosk): keep the grouping the same.
void AdminUpdates::applyAvailable(const QJsonObject &pkgs)
{
    m_available = pkgs;
    m_updatesKnown = true;

    QJsonObject calaosSubset;
    QMap<QString, QStringList> groups;
    UpdateGrouping::splitPackages(pkgs, calaosSubset, groups);

    //no installed list through the relay: the rows are the pending updates
    m_packages->clear();
    m_packages->mergeAvailable(calaosSubset);
    m_groups->setGroups(groups);
}

bool AdminUpdates::recentJob() const
{
    if (!m_job.valid || !m_job.isTerminal())
        return false;
    if (m_seenRunning.contains(m_job.id))
        return true;
    return m_job.finishedAt.isValid() &&
           m_job.finishedAt.secsTo(QDateTime::currentDateTimeUtc()) < JobNewsWindowSecs;
}

void AdminUpdates::recompute()
{
    QString phase;
    if (isUpgrading())
        phase = QStringLiteral("upgrading");
    else if (!m_updatesKnown && recentJob())
    {
        if (m_job.status == QLatin1String("error"))
            phase = QStringLiteral("failed");
        else if (m_job.rebootRequired)
            phase = QStringLiteral("doneReboot");
        else
            phase = QStringLiteral("done");
    }
    else if (!m_updatesKnown)
        phase = QStringLiteral("unknown");
    else
        phase = m_available.isEmpty() ? QStringLiteral("upToDate") : QStringLiteral("available");

    update_updatesCount(m_updatesKnown ? int(m_available.count()) : 0);
    update_totalPercent(qBound(0, int(m_progress.value(QStringLiteral("total_percent")).toDouble()), 100));
    const int index = m_progress.value(QStringLiteral("pkg_index")).toInt();
    update_pkgIndex(index);
    update_pkgCount(m_progress.value(QStringLiteral("pkg_count")).toInt());

    QString pkg;
    if (m_relay->isUnlocked() && phase == QLatin1String("upgrading"))
    {
        pkg = m_progress.value(QStringLiteral("current_pkg")).toString();
        if (pkg.isEmpty() && index >= 1 && index <= m_packageNames.size())
            pkg = m_packageNames.at(index - 1);
    }
    update_currentPackage(pkg);
    update_externalRun(phase == QLatin1String("upgrading") && m_job.valid && m_job.id != m_ownJobId);

    QString failure;
    if (phase == QLatin1String("failed"))
    {
        //the household view reduces every error to these two
        const QString reason = !m_job.error.isEmpty() ? m_job.error
                                                      : m_progress.value(QStringLiteral("error")).toString();
        if (reason == QLatin1String("interrupted"))
            failure = tr("The update was interrupted before the end. Try again.");
        else if (!reason.isEmpty() && reason != QLatin1String("upgrade failed"))
            failure = tr("Reason: %1").arg(reason);
        else
            failure = tr("Try again. If it fails again, the detail is in the box's logs.");
    }
    update_failureText(failure);
    update_phase(phase);
}

void AdminUpdates::forgetBoxData()
{
    m_available = QJsonObject();
    m_updatesKnown = false;
    m_packageNames.clear();
    m_namesAskedFor = 0;
    m_packages->clear();
    m_groups->setGroups({});
    m_progressRows->clear();
    update_boxName(QString());

    //down to the household view, as update_status answers without a session
    m_progress.remove(QStringLiteral("current_pkg"));
    m_progress.remove(QStringLiteral("message"));
    QJsonArray packages;
    for (const QJsonValue &v: m_progress.value(QStringLiteral("packages")).toArray())
    {
        const QJsonObject p = v.toObject();
        packages.append(QJsonObject{{ QStringLiteral("state"), p.value(QStringLiteral("state")) },
                                    { QStringLiteral("percent"), p.value(QStringLiteral("percent")) }});
    }
    if (m_progress.contains(QStringLiteral("packages")))
        m_progress[QStringLiteral("packages")] = packages;
    if (!m_progress.value(QStringLiteral("error")).toString().isEmpty())
        m_progress[QStringLiteral("error")] = QStringLiteral("upgrade failed");
    m_job.package.clear();
    if (!m_job.error.isEmpty() && m_job.error != QLatin1String("interrupted"))
        m_job.error = QStringLiteral("upgrade failed");

    recompute();
}
