#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QElapsedTimer>
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
 * An upgrade ends on the terminal state of its /api/v2 job, or on the legacy
 * synchronous POST result; status snapshots only conclude runs we do not own
 * (resync mode: adopted, or our legacy POST severed by a daemon restart).
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
    //fallback when a self-upgrading daemon never drops the events connection
    void setDaemonRestartTimeout(int ms);

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
    bool isJobOfOurPost(const CalaosOsProtocol::Job &job) const;
    //follow or conclude the job our unanswered POST started; false if it is not one
    bool claimJobOfOurPost(const CalaosOsProtocol::Job &job);
    void handleJobAction(CalaosOsProtocol::UpgradeJobTracker::Action action);
    void concludeJob(const CalaosOsProtocol::Job &job);
    void finishJob(const CalaosOsProtocol::Job &job);
    void resumeAfterDaemonRestart();
    void followForeignJob(const QString &id, const QString &message);
    //notifyWhenNothingToDo false: keep a warning already shown on screen
    void showPostUpgradeDialog(const QSet<QString> &pkgs, bool notifyWhenNothingToDo = true);
    void keepInstalledPart();
    bool endUpgrade();
    void finishUnclaimed(const QString &message);
    void applyStatusSnapshot(const QJsonObject &st);
    void adoptInFlightUpgrade(const QJsonObject &st);
    void finishUpgrade(bool ok, const QString &err);

    void ensureStatusStream(bool delayReconnect = false);
    void startStatusPolling();
    void stopStatusPolling();
    void startJobPolling();
    void handleStatusReply(quint64 gen, const CalaosOsProtocol::UpdateStatus &status);
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
    bool m_statusInFlight = false;  //no second read from progress frames meanwhile
    QElapsedTimer m_lastStatusRead; //a read just answered: wsConnected skips its own
    bool m_wsWasConnected = false; //only a drop of a working connection may mean a new daemon

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

    //last job the daemon showed, to tell a job our severed POST may have started
    QString m_lastSeenJobId;
    QString m_jobIdBeforePost;
    QDateTime m_postSentAt;
    QDateTime m_postFailedAt;
    bool m_postOutcomeUnknown = false; //v2 POST got no answer: a job may or may not exist

    //following a run of another client: never claim it installed our packages
    bool m_foreignRun = false;
    QString m_foreignMessage;
    QSet<QString> m_installedBeforeForeign;

    //the job replaced the daemon binary: wait for the new daemon before going on
    bool m_awaitingRestart = false;
    bool m_restartDropped = false;
    bool m_resumingAfterRestart = false; //the status read after the restart is in flight
    bool m_statusFromNewDaemon = false; //handling a status read that shows the daemon at rest
    CalaosOsProtocol::Job m_restartJob;
    QTimer *m_restartTimer = nullptr;
};

#endif // UPDATEMANAGER_H
