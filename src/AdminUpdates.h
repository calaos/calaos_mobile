#ifndef ADMINUPDATES_H
#define ADMINUPDATES_H

#include <QObject>
#include <QAbstractItemModel>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include "qqmlhelpers.h"
#include "UpdateBackend.h"
#include "UpdateModel.h"
#include "AdminRelay.h"

class QTimer;

/*
 * The "Box updates" screen of the mobile app: the update flow of the webapp's
 * UpdatesPanel (calaos-web-app, stores/admin.ts) over an UpdateBackend, behind
 * the admin gate of AdminRelay. Feeds the shared update models
 * (UpdatePackagesModel for calaos-* packages, UpdateGroupsModel for the
 * others, UpdateProgressModel) like the kiosk's UpdateManager does.
 *
 * update_status is the probe and the source of truth after a reconnect:
 * events are not replayed. Package names only come with an admin session:
 * the household view (no session) is reduced to the progress figures.
 */
class AdminUpdates: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel *packagesModel READ packagesModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *groupsModel READ groupsModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *progressModel READ progressModel CONSTANT)

    //AdminRelay gate, see AdminRelay::Gate*
    QML_READONLY_PROPERTY(QString, gate)
    QML_READONLY_PROPERTY(bool, isUnlocked)
    //"" or the request holding the screen: unlock, update_check,
    //update_upgrade_all, system_reboot
    QML_READONLY_PROPERTY(QString, busy)
    //unknown | upToDate | available | upgrading | done | doneReboot | failed
    QML_READONLY_PROPERTY(QString, phase)
    QML_READONLY_PROPERTY(int, updatesCount)
    QML_READONLY_PROPERTY(int, totalPercent)
    QML_READONLY_PROPERTY(QString, currentPackage)
    QML_READONLY_PROPERTY(int, pkgIndex)
    QML_READONLY_PROPERTY(int, pkgCount)
    //the running upgrade was started elsewhere (kiosk, webapp, CLI)
    QML_READONLY_PROPERTY(bool, externalRun)
    //why the last run failed, "failed" phase only
    QML_READONLY_PROPERTY(QString, failureText)
    //error of the last action, shown under it; empty when none
    QML_READONLY_PROPERTY(QString, errorText)
    QML_READONLY_PROPERTY(QString, errorCode)
    //rate_limited: seconds before the password may be tried again
    QML_READONLY_PROPERTY(int, retryAfter)
    //"" | reboot_sent | session_invalidated
    QML_READONLY_PROPERTY(QString, info)
    //daemon_too_old lasted 2 min: only a reboot will do
    QML_READONLY_PROPERTY(bool, rebootHint)
    //system_info of the box once unlocked
    QML_READONLY_PROPERTY(QString, boxName)

public:
    AdminUpdates(UpdateBackend *backend, AdminRelay *relay, QObject *parent = nullptr);

    QAbstractItemModel *packagesModel() { return m_packages; }
    QAbstractItemModel *groupsModel() { return m_groups; }
    QAbstractItemModel *progressModel() { return m_progressRows; }
    UpdatePackagesModel *packages() { return m_packages; }
    UpdateGroupsModel *groups() { return m_groups; }
    UpdateProgressModel *progressRows() { return m_progressRows; }

    //The screen is shown: probe, and keep retrying a blocked gate meanwhile.
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    //Probe again now (gates without automatic retry).
    Q_INVOKABLE void retry();
    Q_INVOKABLE void unlock(const QString &password);
    Q_INVOKABLE void checkUpdates();
    Q_INVOKABLE void upgradeAll();
    Q_INVOKABLE void reboot();
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE void dismissInfo();

    bool isUpgrading() const;
    //The screen is shown (between activate() and deactivate()).
    bool isActive() const { return m_active; }
    //How long a password typed during a reconnection waits for the box.
    static constexpr int HeldUnlockMs = 30000;
    void setHeldUnlockTimeout(int ms);

    //What the user reads for an admin error code (webapp wording).
    static QString errorTextFor(const QString &code, const QString &msg, int retryAfter);

    //Tests shorten the delays.
    void setPollInterval(int ms);
    void setRetryDelays(int fastMs, int slowMs);

private:
    void probe();
    void refreshStatus();
    void listAvailable();
    void readSystemInfo();
    void applyStatus(const CalaosOsProtocol::UpdateStatus &status);
    void applyProgress(const QJsonObject &progress);
    void onProgressEvent(const QJsonObject &status);
    void applyAvailable(const QJsonObject &pkgs);
    QJsonObject held(const QJsonObject &next) const;
    bool recentJob() const;
    void recompute();
    void forgetBoxData();
    void onGateChanged();
    void scheduleRetry();
    void showError(const CalaosOsReply &reply);
    void showError(const QString &code, const QString &msg);
    void startCountdown(int seconds);
    void holdUnlock(const QString &password);
    void dropHeldUnlock();
    void sendHeldUnlock();
    //An error that moved the gate is shown by the gate screen, not inline.
    static bool isGateCode(const QString &code);

    UpdateBackend *m_backend;
    AdminRelay *m_relay;
    UpdatePackagesModel *m_packages;
    UpdateGroupsModel *m_groups;
    UpdateProgressModel *m_progressRows;

    bool m_active = false;
    bool m_updatesKnown = false;
    QJsonObject m_available;
    QJsonObject m_progress;
    CalaosOsProtocol::Job m_job;
    QString m_ownJobId;
    QSet<QString> m_seenRunning;
    QStringList m_packageNames; //packages[].name of the last full read
    int m_namesAskedFor = 0;
    int m_statusReads = 0;
    bool m_rereadAfter = false;

    QTimer *m_pollTimer = nullptr;
    QTimer *m_retryTimer = nullptr;
    QTimer *m_rebootHintTimer = nullptr;
    QTimer *m_countdownTimer = nullptr;
    //A password that arrived while the socket was being reopened (the
    //password manager sent the app to the background): memory only, for
    //HeldUnlockMs at most, for the box it was typed for.
    QString m_heldPassword;
    QString m_heldHost;
    QTimer *m_heldTimer = nullptr;
    qint64 m_unavailableSince = 0;
    int m_retryAttempt = 0;
    int m_fastRetryMs = 5000;
    int m_slowRetryMs = 60000;
};

#endif // ADMINUPDATES_H
