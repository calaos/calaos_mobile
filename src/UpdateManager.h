#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QQmlApplicationEngine>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include "qqmlhelpers.h"
#include "UpdateModel.h"
#include "CalaosOsAPI.h"

/*
 * Orchestration core of the software-update feature (desktop only).
 *
 * Talks to the calaos-container update daemon through CalaosOsAPI and feeds
 * the models exposed to QML: UpdatePackagesModel (individual calaos-*
 * packages), UpdateGroupsModel (synthetic "kernel"/"system" buckets, see
 * UpdateGrouping) and UpdateProgressModel (per-package progress rows of the
 * ongoing upgrade).
 *
 * Upgrade state machine, single source of truth rules:
 *  - On /api/v2 the upgrade POST answers 202 with a job at once. Only the
 *    terminal state of that job ends the upgrade, as seen on an upgrade_job
 *    event or on update/status, read every 15 s as a safety net because
 *    events are best effort. Progress frames are display-only for a job.
 *  - On the legacy surface (daemon without /api/v2, or without jobs) the
 *    upgrade POST is synchronous server-side: it returns only when the whole
 *    apt transaction ends and is the ONLY thing that advances/finishes an
 *    upgrade we initiated. It also carries pre-start rejections (lock held,
 *    package not found) as HTTP errors, which never produce WebSocket events.
 *  - A transport-severed POST (httpStatus 0, no HTTP reply) is NOT a
 *    rejection: upgrade-all installs calaos-container first, which restarts
 *    the daemon and kills the in-flight POST while the transaction goes on.
 *    We then switch to resync mode and let the status stream conclude.
 *  - WebSocket/poll status snapshots are display-only for upgrades we own.
 *    They finish the upgrade ONLY in resync mode, i.e. when we own no POST:
 *    the app restarted mid-upgrade, another client started the upgrade, or
 *    our POST was severed. Once the job of such a run is known, its terminal
 *    state concludes it instead.
 *  - Each status frame is a full snapshot: local progress state is replaced
 *    wholesale. After a terminal done/error the daemon publishes one trailing
 *    "idle" frame: a no-op for a finished upgrade, but in resync mode it
 *    also concludes the upgrade when the terminal frame itself was missed
 *    (daemon restart, poll-only visibility).
 */
class UpdateManager : public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(QString, updateState) // "idle" | "checking" | "upgrading" | "error"
    QML_READONLY_PROPERTY(bool, isChecking)
    QML_READONLY_PROPERTY(bool, isUpgrading)
    QML_READONLY_PROPERTY(int, updatesCount)
    QML_READONLY_PROPERTY(bool, hasUpdates)
    QML_READONLY_PROPERTY(bool, daemonAvailable)
    QML_READONLY_PROPERTY(bool, installedListAvailable)
    QML_READONLY_PROPERTY(QString, lastErrorString)
    //the last upgrade job installed a kernel/firmware: a reboot finishes it
    QML_READONLY_PROPERTY(bool, rebootRequired)

    //progress of the ongoing upgrade, mirrored from the daemon status snapshots
    QML_READONLY_PROPERTY(int, totalPercent)
    QML_READONLY_PROPERTY(QString, currentPackage)
    QML_READONLY_PROPERTY(QString, currentStep)
    QML_READONLY_PROPERTY(int, currentPercent)
    QML_READONLY_PROPERTY(int, pkgIndex)
    QML_READONLY_PROPERTY(int, pkgCount)

public:
    //api: the daemon client, CalaosOsAPI::Instance() unless a test injects its own
    explicit UpdateManager(QQmlApplicationEngine *engine, QObject *parent = nullptr, CalaosOsAPI *api = nullptr);

    UpdatePackagesModel *calaosModel();
    UpdateGroupsModel *groupsModel();
    UpdateProgressModel *progressModel();

    //manual "check for updates" (server-side apt-get update, can take tens of seconds)
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void upgradePackage(QString name);
    Q_INVOKABLE void upgradeGroup(QString groupId);
    Q_INVOKABLE void upgradeAll();
    Q_INVOKABLE void clearError();

    //safety-net read of update/status while a job runs (tests shorten it)
    void setJobPollInterval(int ms);

private slots:
    void connectWs();
    void openWs();
    void wsConnected();
    void wsClosed();
    void wsTextMessageReceived(const QString &msg);
    void pollStatus();

private:
    void doCheck(bool automatic);
    void applyAvailable(const QJsonObject &pkgs);
    void refreshInstalledList();
    void maybeNotifyUpdates(const QJsonObject &pkgs);

    void beginUpgrade(const QStringList &queue, bool usesUpgradeAll);
    void postNextInQueue();
    void continueQueue();
    void handleUpgradeReply(const CalaosOsReply &reply);
    void handleJobAction(CalaosOsProtocol::UpgradeJobTracker::Action action);
    void concludeJob(const CalaosOsProtocol::Job &job);
    void applyStatusSnapshot(const QJsonObject &st);
    void adoptInFlightUpgrade(const QJsonObject &st);
    void finishUpgrade(bool ok, const QString &err);

    void startStatusPolling();
    void stopStatusPolling();
    void startJobPolling();
    void scheduleWsReconnect();

    QQmlApplicationEngine *engine;
    CalaosOsAPI *m_api;

    UpdatePackagesModel *m_calaosModel = nullptr;
    UpdateGroupsModel *m_groupsModel = nullptr;
    UpdateProgressModel *m_progressModel = nullptr;

    //last known set of available updates: {name: {name, version, current_version}}
    QJsonObject m_available;

    //WebSocket to the daemon event stream + reconnect/poll fallback machinery
    QWebSocket m_ws;
    QTimer *m_wsReconnectTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_jobPollTimer = nullptr;
    bool m_wsDetecting = false;    //API version probe in flight before opening the WS
    int m_wsBackoffMs = 1000;
    bool m_expectSnapshot = false; //next WS frame is the connect-time snapshot (may be stale)
    bool m_wsGotSnapshot = false;  //a connect-time snapshot was ever received
    quint64 m_pollGeneration = 0;  //bumped on finishUpgrade to drop in-flight poll replies
    bool m_inStatusReply = false;  //handling an update/status reply

    //state of the ongoing upgrade
    QStringList m_queue;      //packages left to POST (single/group upgrade loop)
    int m_queueIndex = 0;
    int m_queueDone = 0;
    int m_queueTotal = 0;
    bool m_usesUpgradeAll = false;
    bool m_finishHandled = true;     //finishUpgrade idempotency guard
    bool m_terminalWasError = false; //a terminal "error" snapshot was seen (resync conclusion on a lone "idle")
    QString m_terminalMessage;       //message of that terminal snapshot, if any
    bool m_resyncMode = false;    //upgrade not owned by a POST (adopted or severed), status stream concludes it
    QSet<QString> m_upgradeSet;   //all packages of the ongoing upgrade (for reboot decision)

    //upgrade job(s) of the ongoing upgrade, /api/v2 only
    CalaosOsProtocol::UpgradeJobTracker m_jobs;
    bool m_jobRebootKnown = false;    //a job ended: its reboot_required replaces the package heuristic
    bool m_jobRebootRequired = false;
};

#endif // UPDATEMANAGER_H
