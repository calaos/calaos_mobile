#include "UpdateManager.h"
#include "CalaosOsAPI.h"
#include "UpdateGrouping.h"
#include "HardwareUtils.h"
#include <qfappdispatcher.h>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QDebug>

UpdateManager::UpdateManager(QQmlApplicationEngine *eng, QObject *parent):
    QObject(parent),
    engine(eng)
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
    update_totalPercent(0);
    update_currentPackage(QString());
    update_currentStep(QString());
    update_currentPercent(0);
    update_pkgIndex(0);
    update_pkgCount(0);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &UpdateManager::pollStatus);

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
    CalaosOsAPI::Instance()->getAvailableUpdates([this](bool ok, const QJsonObject &pkgs, const QString &)
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

    CalaosOsAPI::Instance()->checkUpdates([this, automatic](bool ok, const QJsonObject &pkgs, const QString &err)
    {
        update_isChecking(false);

        if (ok)
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
            if (err.contains(QStringLiteral("in progress"), Qt::CaseInsensitive))
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
            if (!err.contains(QStringLiteral("in progress"), Qt::CaseInsensitive))
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
    CalaosOsAPI::Instance()->getInstalledPackages([this](bool ok, int, const QJsonObject &pkgs)
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
    QString summary;
    if (calaosNames.size() > 5)
        summary = QStringList(calaosNames.mid(0, 5)).join(QStringLiteral(", ")) + QStringLiteral("…");
    else
        summary = calaosNames.join(QStringLiteral(", "));

    QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
    QVariantMap m = {{ "count", pkgs.count() },
                     { "calaosCount", calaosNames.count() },
                     { "summary", summary }};
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
        CalaosOsAPI::Instance()->upgradeAll([this](bool ok, int httpStatus, const QString &err)
        {
            if (m_finishHandled)
                return;

            if (!ok && httpStatus == 0)
            {
                //transport severed, not a rejection: upgrade-all installs
                //calaos-container first, which restarts the daemon and kills
                //this in-flight POST while the transaction goes on. Switch to
                //resync mode: the status stream (WS/poll) concludes the upgrade
                m_resyncMode = true;
                if (m_ws.state() != QAbstractSocket::ConnectedState &&
                    m_ws.state() != QAbstractSocket::ConnectingState)
                    scheduleWsReconnect();
                startStatusPolling();
                return;
            }

            finishUpgrade(ok, err);
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

    //the POST returns only when the whole apt transaction is over: its result
    //is authoritative and is the only thing advancing the queue
    CalaosOsAPI::Instance()->upgradePackage(pkg, [this](bool ok, int httpStatus, const QString &err)
    {
        if (m_finishHandled)
            return;

        //re-enter the queue through the event loop so a deep queue (or a
        //synchronous request-start failure) can never recurse
        auto continueQueue = [this]()
        {
            QMetaObject::invokeMethod(this, [this]()
            {
                if (!m_finishHandled)
                    postNextInQueue();
            }, Qt::QueuedConnection);
        };

        if (ok)
        {
            m_queueDone++;
            continueQueue();
            return;
        }

        if (httpStatus == 0)
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

        if (err.contains(QStringLiteral("package not found"), Qt::CaseInsensitive) &&
            m_queueTotal > 1)
        {
            //the package was already pulled in as a dependency of an earlier
            //transaction of this queue: treat as done and continue
            m_queueDone++;
            continueQueue();
            return;
        }

        finishUpgrade(false, err);
    });
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
    if (status == QStringLiteral("idle"))
    {
        if (m_resyncMode && get_isUpgrading())
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
        //snapshot terminal only concludes an adopted (resync) upgrade
        if (m_resyncMode)
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

    applyStatusSnapshot(st);
}

void UpdateManager::finishUpgrade(bool ok, const QString &err)
{
    if (m_finishHandled)
        return;
    m_finishHandled = true;

    stopStatusPolling();
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

        CalaosOsAPI::Instance()->getAvailableUpdates([this](bool ok2, const QJsonObject &pkgs, const QString &)
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

        if (m_upgradeSet.contains(QStringLiteral("calaos-home")))
        {
            QVariantMap m = {{ "showMachine", hasKernel },
                             { "showApp", true }};
            appDispatcher->dispatch("showRebootDialog", m);
        }
        else if (hasKernel)
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

    QString addr = CalaosOsAPI::Instance()->getBaseAddr();
    if (addr.startsWith(QStringLiteral("https://")))
        addr.replace(0, 8, QStringLiteral("wss://"));
    else if (addr.startsWith(QStringLiteral("http://")))
        addr.replace(0, 7, QStringLiteral("ws://"));

    m_ws.open(QUrl(addr + QStringLiteral("/api/events?token=") +
                   CalaosOsAPI::Instance()->getToken()));
}

void UpdateManager::wsConnected()
{
    m_wsBackoffMs = 1000;
    //the server immediately sends one snapshot of the current state, which
    //may be stale: it is only adopted if an upgrade is actually running
    m_expectSnapshot = true;

    if (m_pollTimer->isActive())
        stopStatusPolling();
}

void UpdateManager::wsClosed()
{
    m_expectSnapshot = false;
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
    if (obj["type"].toString() != QStringLiteral("upgrade_progress"))
        return;

    const QJsonObject data = obj["data"].toObject();

    if (m_expectSnapshot)
    {
        //first frame after connect: the connect-time snapshot
        m_expectSnapshot = false;
        m_wsGotSnapshot = true;

        if (data["status"].toString() == QStringLiteral("upgrading"))
        {
            if (!get_isUpgrading())
                adoptInFlightUpgrade(data); //resync after app restart
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
            adoptInFlightUpgrade(data);
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

    CalaosOsAPI::Instance()->getUpdateStatus([this, gen](bool ok, const QJsonObject &status)
    {
        if (!ok)
            return;
        if (gen != m_pollGeneration)
            return; //reply from before the upgrade finished, drop it

        if (!get_isUpgrading())
        {
            //resync rule: an upgrade is running that we do not know about
            if (status["status"].toString() == QStringLiteral("upgrading"))
                adoptInFlightUpgrade(status);
            return;
        }

        applyStatusSnapshot(status);
    });
}
