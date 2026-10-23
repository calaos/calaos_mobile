#include "UpdateManager.h"
#include "UpdateGrouping.h"
#include "HardwareUtils.h"
#include <qfappdispatcher.h>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QDebug>
#include <QScopedValueRollback>

using namespace CalaosOsProtocol;

UpdateManager::UpdateManager(QQmlApplicationEngine *eng, QObject *parent, CalaosOsAPI *api):
    QObject(parent),
    engine(eng),
    m_api(api ? api : CalaosOsAPI::Instance())
{
    m_calaosModel = new UpdatePackagesModel(this);
    m_groupsModel = new UpdateGroupsModel(this);
    m_progressModel = new UpdateProgressModel(this);

    update_updateState(QStringLiteral("idle"));
    update_isChecking(false);
    update_isUpgrading(false);
    update_updatesCount(0);
    update_hasUpdates(false);
    update_daemonAvailable(true);
    update_installedListAvailable(false);
    update_lastErrorString(QString());
    update_rebootRequired(false);
    update_totalPercent(0);
    update_currentPackage(QString());
    update_currentStep(QString());
    update_currentPercent(0);
    update_pkgIndex(0);
    update_pkgCount(0);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &UpdateManager::pollStatus);

    //a subscriber that falls behind loses events, the terminal upgrade_job
    //included: update/status is the source of truth
    m_jobPollTimer = new QTimer(this);
    m_jobPollTimer->setInterval(15000);
    connect(m_jobPollTimer, &QTimer::timeout, this, &UpdateManager::pollStatus);

    m_wsReconnectTimer = new QTimer(this);
    m_wsReconnectTimer->setSingleShot(true);
    connect(m_wsReconnectTimer, &QTimer::timeout, this, &UpdateManager::connectWs);

    connect(&m_ws, &QWebSocket::connected, this, &UpdateManager::wsConnected);
    connect(&m_ws, &QWebSocket::disconnected, this, &UpdateManager::wsClosed);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(&m_ws, &QWebSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) { wsClosed(); });
#else
    connect(&m_ws, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, [this](QAbstractSocket::SocketError) { wsClosed(); });
#endif
    connect(&m_ws, &QWebSocket::textMessageReceived, this, &UpdateManager::wsTextMessageReceived);

    //initial data: updates found by a previous daemon check + installed packages
    m_api->getAvailableUpdates([this](bool ok, const QJsonObject &pkgs, const QString &)
    {
        update_daemonAvailable(ok);
        if (ok)
        {
            update_lastErrorString(QString());
            applyAvailable(pkgs);
        }
    });
    refreshInstalledList();

    //first automatic check 2 minutes after startup, then every 12 hours
    QTimer::singleShot(120000, this, [this]() { doCheck(true); });

    auto checkTimer = new QTimer(this);
    connect(checkTimer, &QTimer::timeout, this, [this]() { doCheck(true); });
    checkTimer->start(12 * 60 * 60 * 1000);

    //live progress events from the daemon
    connectWs();

    //if the WS could not deliver its connect-time snapshot quickly, do one
    //status poll to catch an upgrade already in flight (app restarted
    //mid-upgrade with the WS down)
    QTimer::singleShot(3000, this, [this]()
    {
        if (!m_wsGotSnapshot)
            pollStatus();
    });
}

UpdatePackagesModel *UpdateManager::calaosModel()
{
    return m_calaosModel;
}

UpdateGroupsModel *UpdateManager::groupsModel()
{
    return m_groupsModel;
}

UpdateProgressModel *UpdateManager::progressModel()
{
    return m_progressModel;
}

void UpdateManager::refresh()
{
    doCheck(false);
}

void UpdateManager::doCheck(bool automatic)
{
    if (get_isUpgrading() || get_isChecking())
        return;

    update_isChecking(true);
    update_updateState(QStringLiteral("checking"));

    m_api->checkUpdates([this, automatic](const CalaosOsReply &reply)
    {
        update_isChecking(false);

        const QJsonObject pkgs = reply.data.toObject();
        const QString &err = reply.error;

        if (reply.ok)
        {
            update_daemonAvailable(true);
            update_lastErrorString(QString());
            applyAvailable(pkgs);
            //also retry the installed list: a previously failed fetch
            //recovers on the next (manual or automatic) successful check
            refreshInstalledList();
            if (automatic)
                maybeNotifyUpdates(pkgs);
            if (get_updateState() == QStringLiteral("checking"))
                update_updateState(QStringLiteral("idle"));
            return;
        }

        if (automatic)
        {
            if (reply.upgradeInProgress)
            {
                //an upgrade holds the apt lock, silently skip this automatic check
            }
            else
            {
                qWarning() << "UpdateManager: automatic update check failed:" << err;
                update_daemonAvailable(false);
            }
            if (get_updateState() == QStringLiteral("checking"))
                update_updateState(QStringLiteral("idle"));
        }
        else
        {
            update_lastErrorString(err.isEmpty() ? tr("Cannot contact the update service") : err);
            update_updateState(QStringLiteral("error"));
            //daemon rejecting the check because an upgrade runs means it is alive
            if (!reply.upgradeInProgress)
                update_daemonAvailable(false);
        }
    });
}

void UpdateManager::applyAvailable(const QJsonObject &pkgs)
{
    m_available = pkgs;

    QJsonObject calaosSubset;
    QMap<QString, QStringList> groups;

    for (auto it = pkgs.begin(); it != pkgs.end(); ++it)
    {
        const QString group = UpdateGrouping::groupForPackage(it.key());
        if (group.isEmpty())
            calaosSubset[it.key()] = it.value();
        else
            groups[group].append(it.key());
    }

    m_calaosModel->mergeAvailable(calaosSubset);
    m_groupsModel->setGroups(groups);

    update_updatesCount(pkgs.count());
    update_hasUpdates(pkgs.count() > 0);
}

void UpdateManager::refreshInstalledList()
{
    //calaos_home runs inside a docker container on Calaos OS: the host's dpkg
    //is not reachable from here, the calaos-container daemon is the only
    //source for the installed package list. If the endpoint is missing (old
    //daemon, 404) or the daemon is unreachable, the UI simply degrades.
    m_api->getInstalledPackages([this](bool ok, int, const QJsonObject &pkgs)
    {
        if (ok)
        {
            m_calaosModel->setInstalled(pkgs);
            update_installedListAvailable(true);
            return;
        }

        update_installedListAvailable(false);
    });
}

void UpdateManager::maybeNotifyUpdates(const QJsonObject &pkgs)
{
    if (pkgs.isEmpty())
        return;

    QStringList entries;
    QStringList calaosNames;
    for (auto it = pkgs.begin(); it != pkgs.end(); ++it)
    {
        entries.append(it.key() + QStringLiteral("=") + it.value().toObject()["version"].toString());
        if (it.key().startsWith(QStringLiteral("calaos-")))
            calaosNames.append(it.key());
    }
    entries.sort();

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(entries.join(QStringLiteral(";")).toUtf8(),
                                 QCryptographicHash::Sha1).toHex());

    //only notify once per distinct set of updates
    if (HardwareUtils::Instance()->getConfigOption(QStringLiteral("update_notified_hash")) == hash)
        return;
    HardwareUtils::Instance()->setConfigOption(QStringLiteral("update_notified_hash"), hash);

    calaosNames.sort();

    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
    QVariantMap m = {{ "count", pkgs.count() },
                     { "calaosCount", calaosNames.count() },
                     { "calaosPackages", calaosNames }};
    appDispatcher->dispatch("showUpdateAvailableDialog", m);
}

void UpdateManager::upgradePackage(QString name)
{
    if (get_isUpgrading() || get_isChecking())
        return;
    if (name.isEmpty())
        return;

    beginUpgrade({name}, false);
}

void UpdateManager::upgradeGroup(QString groupId)
{
    if (get_isUpgrading() || get_isChecking())
        return;

    UpdateGroupItem *item = m_groupsModel->itemForGroup(groupId);
    if (!item)
        return;
    const QStringList queue = item->get_groupPackages();
    if (queue.isEmpty())
        return;

    beginUpgrade(queue, false);
}

void UpdateManager::upgradeAll()
{
    if (get_isUpgrading() || get_isChecking())
        return;
    if (m_available.isEmpty())
        return;

    beginUpgrade(m_available.keys(), true);
}

void UpdateManager::beginUpgrade(const QStringList &queue, bool usesUpgradeAll)
{
    m_queue = queue;
    m_queueIndex = 0;
    m_queueDone = 0;
    m_queueTotal = queue.size();
    m_usesUpgradeAll = usesUpgradeAll;
    m_finishHandled = false;
    m_terminalWasError = false;
    m_terminalMessage.clear();
    m_resyncMode = false;
    m_upgradeSet = QSet<QString>(queue.begin(), queue.end());
    m_jobs.clear();
    m_jobRebootKnown = false;
    m_jobRebootRequired = false;

    update_isUpgrading(true);
    update_updateState(QStringLiteral("upgrading"));
    update_totalPercent(0);
    update_currentPackage(QString());
    update_currentStep(QString());
    update_currentPercent(0);
    update_pkgIndex(0);
    update_pkgCount(0);

    m_progressModel->clear();
    m_calaosModel->clearProgress();
    m_groupsModel->clearProgress();

    //make sure progress can reach us: WS preferred, polling as belt-and-braces
    //while the WS is not established
    if (m_ws.state() != QAbstractSocket::ConnectedState &&
        m_ws.state() != QAbstractSocket::ConnectingState)
    {
        connectWs();
        startStatusPolling();
    }

    if (usesUpgradeAll)
    {
        m_jobs.startPending();
        m_api->upgradeAll([this](const CalaosOsReply &reply)
        {
            if (m_finishHandled)
                return;
            handleUpgradeReply(reply);
        });
    }
    else
    {
        postNextInQueue();
    }
}

void UpdateManager::postNextInQueue()
{
    if (m_queueIndex >= m_queue.size())
    {
        finishUpgrade(true, QString());
        return;
    }

    const QString pkg = m_queue.at(m_queueIndex);
    m_queueIndex++;

    //each package is one apt transaction: a legacy POST that returns when it
    //is over, or a job whose terminal state advances the queue
    m_jobs.startPending();
    m_api->upgradePackage(pkg, [this](const CalaosOsReply &reply)
    {
        if (m_finishHandled)
            return;
        handleUpgradeReply(reply);
    });
}

void UpdateManager::continueQueue()
{
    //re-enter the queue through the event loop so a deep queue (or a
    //synchronous request-start failure) can never recurse
    QMetaObject::invokeMethod(this, [this]()
    {
        if (!m_finishHandled)
            postNextInQueue();
    }, Qt::QueuedConnection);
}

void UpdateManager::handleUpgradeReply(const CalaosOsReply &reply)
{
    const bool queued = !m_usesUpgradeAll;

    if (reply.api == ApiVersion::V2 && reply.ok)
    {
        //202: the run goes on as a job, only its terminal state ends it
        const Job job = parseJob(reply.data);
        if (!job.valid)
        {
            m_jobs.clear();
            finishUpgrade(false, tr("Invalid answer from the update service"));
            return;
        }

        m_pollGeneration++; //a status read sent before the POST knows an older job
        startJobPolling();
        handleJobAction(m_jobs.follow(job.id));
        return;
    }

    if (reply.api == ApiVersion::V2 && reply.upgradeInProgress)
    {
        //another client's run holds the lock: show it rather than fail
        const QString runningId = reply.data.toObject()["job_id"].toString();
        if (!runningId.isEmpty())
        {
            m_resyncMode = true;
            m_usesUpgradeAll = false;
            m_queue.clear();
            m_queueIndex = 0;
            m_queueDone = 0;
            m_queueTotal = 0;
            m_pollGeneration++;
            startJobPolling();
            handleJobAction(m_jobs.follow(runningId));
            pollStatus();
            return;
        }

        m_jobs.clear();
        finishUpgrade(false, tr("Another update operation is in progress, try again later."));
        return;
    }

    m_jobs.clear();

    if (reply.ok)
    {
        //legacy synchronous POST: the whole apt transaction is over, its
        //result is authoritative and is the only thing advancing the queue
        if (queued)
        {
            m_queueDone++;
            continueQueue();
        }
        else
            finishUpgrade(true, QString());
        return;
    }

    if (reply.httpStatus == 0)
    {
        //transport error (connection severed/refused, no HTTP reply):
        //this is NOT an authoritative rejection, the transaction may
        //still be running (e.g. calaos-container upgraded itself and
        //restarted the daemon). Switch to resync mode: the status
        //stream (WS/poll) concludes the upgrade
        m_resyncMode = true;
        if (m_ws.state() != QAbstractSocket::ConnectedState &&
            m_ws.state() != QAbstractSocket::ConnectingState)
            scheduleWsReconnect();
        startStatusPolling();
        return;
    }

    if (queued && reply.packageNotFound && m_queueTotal > 1)
    {
        //the package was already pulled in as a dependency of an earlier
        //transaction of this queue: treat as done and continue
        m_queueDone++;
        continueQueue();
        return;
    }

    finishUpgrade(false, reply.error);
}

void UpdateManager::handleJobAction(UpgradeJobTracker::Action action)
{
    switch (action)
    {
    case UpgradeJobTracker::Action::None:
        break;
    case UpgradeJobTracker::Action::Adopt:
        startJobPolling();
        break;
    case UpgradeJobTracker::Action::Finish:
    {
        //a copy: finishing clears the tracker that holds it
        const Job finished = m_jobs.finishedJob();
        concludeJob(finished);
        break;
    }
    case UpgradeJobTracker::Action::Lost:
        finishUpgrade(false, tr("The update service lost track of the update"));
        break;
    }
}

void UpdateManager::concludeJob(const Job &job)
{
    m_jobPollTimer->stop();
    m_jobRebootKnown = true;
    m_jobRebootRequired = m_jobRebootRequired || job.rebootRequired;
    update_rebootRequired(m_jobRebootRequired);

    //a job of our own package queue: the next package follows it
    if (job.succeeded() && !m_resyncMode && !m_usesUpgradeAll && m_queueTotal > 0)
    {
        m_queueDone++;
        continueQueue();
        return;
    }

    finishUpgrade(job.succeeded(), job.error);
}

void UpdateManager::applyStatusSnapshot(const QJsonObject &st)
{
    const QString status = st["status"].toString();

    //"idle" frames carry nothing to display: for an upgrade we own this is
    //the trailing frame the daemon publishes after done/error, or a stale
    //state, and is ignored (finishUpgrade is idempotent anyway). In resync
    //mode the status stream is the only finisher and the terminal frame
    //itself may have been missed (daemon restart, poll-only visibility):
    //conclude with the last observed terminal outcome, success if none
    if (isIdleProgress(st))
    {
        if (m_jobs.isFollowing())
        {
            //the run is over: read its job, its terminal event may be lost
            if (!m_inStatusReply)
                pollStatus();
        }
        else if (m_resyncMode && get_isUpgrading())
            finishUpgrade(!m_terminalWasError, m_terminalMessage);
        return;
    }

    //each frame is a full snapshot: replace the local state wholesale
    //(missing keys decode as 0/"")
    const int snapTotal = st["total_percent"].toInt();
    update_currentPackage(st["current_pkg"].toString());
    update_currentStep(st["current_step"].toString());
    update_currentPercent(st["current_percent"].toInt());
    update_pkgIndex(st["pkg_index"].toInt());
    update_pkgCount(st["pkg_count"].toInt());

    QMap<QString, QList<int>> groupPercents;
    QMap<QString, int> groupDone;
    QMap<QString, int> groupFailed;

    const QJsonArray packages = st["packages"].toArray();

    //per-package rows of the progress panel, updated in place
    m_progressModel->syncPackages(packages);

    for (const QJsonValue &v: packages)
    {
        const QJsonObject p = v.toObject();
        const QString name = p["name"].toString();
        const QString state = p["state"].toString();
        const int percent = p["percent"].toInt();

        //keep the upgrade set fed from snapshots: an early adoption snapshot
        //may have had an empty packages[], and dependencies pulled into the
        //transaction (e.g. calaos-home) only ever show up here. The set
        //drives the post-success reboot/restart decision
        if (!name.isEmpty())
            m_upgradeSet.insert(name);

        const QString group = UpdateGrouping::groupForPackage(name);
        if (group.isEmpty())
        {
            m_calaosModel->setPkgProgress(name, state, percent);
        }
        else
        {
            groupPercents[group].append(percent);
            if (state == QStringLiteral("done"))
                groupDone[group]++;
            else if (state == QStringLiteral("failed"))
                groupFailed[group]++;
        }
    }

    //mirror an aggregated progress on the group rows. A multi-POST queue
    //(group upgrade) spans several apt transactions and each snapshot only
    //covers the current one: only allow the "done" state once the queue is
    //on its last transaction, otherwise the group row would flash "done"
    //after every intermediate transaction
    const bool queueOnLastTransaction = m_resyncMode || m_usesUpgradeAll ||
                                        m_queueTotal == 0 || m_queueIndex >= m_queueTotal;
    for (auto it = groupPercents.begin(); it != groupPercents.end(); ++it)
    {
        const QList<int> &percents = it.value();
        int sum = 0;
        for (int p: percents)
            sum += p;
        const int avg = percents.isEmpty() ? 0 : sum / int(percents.size());

        QString gstate = QStringLiteral("installing");
        if (groupFailed.value(it.key()) > 0)
            gstate = QStringLiteral("failed");
        else if (groupDone.value(it.key()) == percents.size() && queueOnLastTransaction)
            gstate = QStringLiteral("done");

        m_groupsModel->setGroupProgress(it.key(), gstate, avg);
    }

    //global percent: our own multi-POST queue (group upgrade) spans several
    //apt transactions, combine finished ones with the current snapshot
    if (get_isUpgrading() && !m_usesUpgradeAll && !m_resyncMode && m_queueTotal > 0)
        update_totalPercent(qBound(0, (m_queueDone * 100 + snapTotal) / qMax(1, m_queueTotal), 100));
    else
        update_totalPercent(qBound(0, snapTotal, 100));

    if (status == QStringLiteral("done") || status == QStringLiteral("error"))
    {
        //remember the terminal outcome: if we only enter resync mode later
        //(severed POST) the trailing "idle" concludes with it
        m_terminalWasError = status == QStringLiteral("error");
        m_terminalMessage = st["error"].toString();

        //for upgrades we initiated the POST result is authoritative; the
        //snapshot terminal only concludes an adopted (resync) upgrade, and
        //a job only ends on its own terminal state
        if (m_jobs.isFollowing())
        {
            if (!m_inStatusReply)
                pollStatus();
        }
        else if (m_resyncMode)
            finishUpgrade(status == QStringLiteral("done"), st["error"].toString());
    }
}

void UpdateManager::adoptInFlightUpgrade(const QJsonObject &st)
{
    //the app (re)started while the daemon is in the middle of an upgrade:
    //adopt it. We own no POST for it, so WS/poll terminal frames conclude it.
    m_resyncMode = true;
    m_finishHandled = false;
    m_terminalWasError = false;
    m_terminalMessage.clear();
    m_usesUpgradeAll = false;
    m_queue.clear();
    m_queueIndex = 0;
    m_queueDone = 0;
    m_queueTotal = 0;
    m_jobRebootKnown = false;
    m_jobRebootRequired = false;

    //applyStatusSnapshot below refills it from the snapshot's packages[]
    m_upgradeSet.clear();

    update_isUpgrading(true);
    update_updateState(QStringLiteral("upgrading"));

    //the status stream is the only finisher of an adopted upgrade: make sure
    //it keeps reaching us, WS preferred, polling while it is not established
    if (m_ws.state() != QAbstractSocket::ConnectedState &&
        m_ws.state() != QAbstractSocket::ConnectingState)
    {
        connectWs();
        startStatusPolling();
    }

    //adopted from an upgrade_job event: no progress to show yet
    if (!st.isEmpty())
        applyStatusSnapshot(st);
}

void UpdateManager::finishUpgrade(bool ok, const QString &err)
{
    if (m_finishHandled)
        return;
    m_finishHandled = true;

    stopStatusPolling();
    m_jobPollTimer->stop();
    m_jobs.clear();
    m_pollGeneration++; //drop poll replies still in flight
    update_isUpgrading(false);

    //reset the progress properties mirrored from the status snapshots
    update_totalPercent(0);
    update_currentPackage(QString());
    update_currentStep(QString());
    update_currentPercent(0);
    update_pkgIndex(0);
    update_pkgCount(0);
    m_progressModel->clear();

    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);

    if (ok)
    {
        update_updateState(QStringLiteral("idle"));

        //optimistic local reset, real data is re-fetched right after
        //(the daemon refreshes its available cache after a successful upgrade)
        m_calaosModel->clearAvailable();

        m_api->getAvailableUpdates([this](bool ok2, const QJsonObject &pkgs, const QString &)
        {
            if (ok2)
                applyAvailable(pkgs);
        });
        refreshInstalledList();

        bool hasKernel = false;
        for (const QString &p: m_upgradeSet)
        {
            if (UpdateGrouping::groupForPackage(p) == QStringLiteral("kernel"))
            {
                hasKernel = true;
                break;
            }
        }

        //a job tells from what it really installed; legacy runs only have
        //the package names to guess from
        const bool needsReboot = m_jobRebootKnown ? m_jobRebootRequired : hasKernel;

        if (m_upgradeSet.contains(QStringLiteral("calaos-home")))
        {
            QVariantMap m = {{ "showMachine", needsReboot },
                             { "showApp", true }};
            appDispatcher->dispatch("showRebootDialog", m);
        }
        else if (needsReboot)
        {
            QVariantMap m = {{ "showMachine", true },
                             { "showApp", false }};
            appDispatcher->dispatch("showRebootDialog", m);
        }
        else
        {
            QVariantMap m = {{ "title", tr("Software update") },
                             { "message", tr("Update installed successfully.") },
                             { "button", tr("Close") }};
            appDispatcher->dispatch("showNotificationMsg", m);
        }
    }
    else
    {
        const QString errMsg = err.isEmpty() ? tr("Upgrade failed") : err;
        update_updateState(QStringLiteral("error"));
        update_lastErrorString(errMsg);

        QVariantMap m = {{ "title", tr("Software update") },
                         { "message", tr("Upgrade failed: %1").arg(errMsg) },
                         { "button", tr("Close") },
                         { "timeout", 0 }};
        appDispatcher->dispatch("showNotificationMsg", m);

        //on error drop the partial progress; on success the "done" states stay
        //visible until the re-fetch above replaces them
        m_calaosModel->clearProgress();
        m_groupsModel->clearProgress();
    }

    m_resyncMode = false;
}

void UpdateManager::setJobPollInterval(int ms)
{
    m_jobPollTimer->setInterval(ms);
}

void UpdateManager::startJobPolling()
{
    if (!m_jobPollTimer->isActive())
        m_jobPollTimer->start();
}

void UpdateManager::clearError()
{
    if (get_isUpgrading() || get_isChecking())
        return;

    update_lastErrorString(QString());
    update_updateState(QStringLiteral("idle"));
}

void UpdateManager::connectWs()
{
    const QAbstractSocket::SocketState st = m_ws.state();
    if (st == QAbstractSocket::ConnectedState ||
        st == QAbstractSocket::ConnectingState)
        return;
    if (st != QAbstractSocket::UnconnectedState)
        m_ws.abort();
    if (m_wsDetecting)
        return;

    //the token may have changed while the daemon was away
    m_api->reloadToken();

    //the event stream path depends on the daemon version
    if (m_api->apiVersion() == ApiVersion::Unknown)
    {
        m_wsDetecting = true;
        m_api->detectApiVersion([this]()
        {
            m_wsDetecting = false;
            openWs();
        });
        return;
    }

    openWs();
}

void UpdateManager::openWs()
{
    const QAbstractSocket::SocketState st = m_ws.state();
    if (st == QAbstractSocket::ConnectedState ||
        st == QAbstractSocket::ConnectingState)
        return;

    m_ws.open(m_api->eventsRequest());
}

void UpdateManager::wsConnected()
{
    m_wsBackoffMs = 1000;
    //the server immediately sends one snapshot of the current state, which
    //may be stale: it is only adopted if an upgrade is actually running
    m_expectSnapshot = true;

    if (m_pollTimer->isActive())
        stopStatusPolling();

    //events are not replayed: the persisted job is the source of truth
    if (m_api->apiVersion() == ApiVersion::V2)
        pollStatus();
}

void UpdateManager::wsClosed()
{
    m_expectSnapshot = false;
    //the daemon may come back as another version (it restarts after
    //upgrading itself): detect again before the next connection
    m_api->resetApiVersion();
    scheduleWsReconnect();

    if (get_isUpgrading())
        startStatusPolling();
}

void UpdateManager::scheduleWsReconnect()
{
    if (m_wsReconnectTimer->isActive())
        return;
    m_wsReconnectTimer->start(m_wsBackoffMs);
    m_wsBackoffMs = qMin(m_wsBackoffMs * 2, 15000);
}

void UpdateManager::wsTextMessageReceived(const QString &msg)
{
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    const QString type = obj["type"].toString();

    if (type == QStringLiteral("upgrade_job"))
    {
        const Job job = parseJob(obj["data"]);
        if (!job.valid)
            return;
        update_rebootRequired(job.rebootRequired);

        const bool adoptedWithoutId = get_isUpgrading() && m_resyncMode &&
                                      !m_jobs.isFollowing() && !m_jobs.isPending();
        const UpgradeJobTracker::Action action = m_jobs.onEvent(job, adoptedWithoutId);

        if (action == UpgradeJobTracker::Action::Adopt)
        {
            m_pollGeneration++; //a status read already in flight may predate this job
            //a run started by another client (webapp, calaos-os CLI)
            if (!get_isUpgrading())
                adoptInFlightUpgrade(QJsonObject());
        }
        handleJobAction(action);
        return;
    }

    if (type != QStringLiteral("upgrade_progress"))
        return;

    const QJsonObject data = obj["data"].toObject();
    const bool v2 = m_api->apiVersion() == ApiVersion::V2;

    if (m_expectSnapshot)
    {
        //first frame after connect: the connect-time snapshot
        m_expectSnapshot = false;
        m_wsGotSnapshot = true;

        if (data["status"].toString() == QStringLiteral("upgrading"))
        {
            if (!get_isUpgrading())
            {
                adoptInFlightUpgrade(data); //resync after app restart
                if (v2)
                    pollStatus(); //learn the job of that run
            }
            else
                applyStatusSnapshot(data);  //display for the upgrade we own
        }
        else if (m_resyncMode && get_isUpgrading())
        {
            //we are waiting for the status stream to conclude a resync
            //upgrade and the daemon already reached its terminal state:
            //this done/error/idle snapshot is the conclusion, not stale
            applyStatusSnapshot(data);
        }
        //else: stale done/error/idle state from a previous run, discard

        return;
    }

    if (!get_isUpgrading())
    {
        //an upgrade initiated by another client (e.g. the calaos-os CLI)
        //while we are connected in steady state: adopt it like a
        //restart-time resync, its terminal frames will conclude it
        if (data["status"].toString() == QStringLiteral("upgrading"))
        {
            adoptInFlightUpgrade(data);
            if (v2)
                pollStatus();
        }
        return;
    }

    applyStatusSnapshot(data);
}

void UpdateManager::startStatusPolling()
{
    if (!m_pollTimer->isActive())
        m_pollTimer->start();
}

void UpdateManager::stopStatusPolling()
{
    m_pollTimer->stop();
}

void UpdateManager::pollStatus()
{
    const quint64 gen = m_pollGeneration;

    m_api->getUpdateStatus([this, gen](bool ok, const UpdateStatus &status)
    {
        if (!ok)
            return;
        if (gen != m_pollGeneration)
            return; //reply from before the upgrade finished, drop it

        //this reply is the fresh read: never chain another one from it
        const QScopedValueRollback<bool> inReply(m_inStatusReply, true);

        if (status.hasJobField)
        {
            //the daemon clears it once the box has rebooted
            update_rebootRequired(status.job.valid && status.job.rebootRequired);

            const UpgradeJobTracker::Action action = m_jobs.onStatus(status.job, true);
            if (action == UpgradeJobTracker::Action::Adopt)
            {
                if (!get_isUpgrading())
                    adoptInFlightUpgrade(status.progress);
                else
                    applyStatusSnapshot(status.progress);
                handleJobAction(action);
                return;
            }
            if (action != UpgradeJobTracker::Action::None)
            {
                handleJobAction(action);
                return;
            }
        }

        if (!get_isUpgrading())
        {
            //resync rule: an upgrade is running that we do not know about
            if (status.progress["status"].toString() == QStringLiteral("upgrading"))
                adoptInFlightUpgrade(status.progress);
            return;
        }

        applyStatusSnapshot(status.progress);
    });
}
