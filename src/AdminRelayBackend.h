#ifndef ADMINRELAYBACKEND_H
#define ADMINRELAYBACKEND_H

#include "UpdateBackend.h"
#include "AdminRelay.h"

//UpdateBackend over calaos_server's admin relay (mobile): each call is one
//admin action, progress comes from admin_event upgrade_progress. Replies
//look like the daemon's /api/v2 ones; reply.code carries the admin error code.
class AdminRelayBackend: public QObject, public UpdateBackend
{
    Q_OBJECT
public:
    explicit AdminRelayBackend(AdminRelay *relay, QObject *parent = nullptr);

    void checkUpdates(std::function<void(const CalaosOsReply &reply)> callback) override;
    void getAvailableUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback) override;
    void upgradePackage(const QString &package, std::function<void(const CalaosOsReply &reply)> callback) override;
    void upgradeAll(std::function<void(const CalaosOsReply &reply)> callback) override;
    void getUpdateStatus(std::function<void(bool ok, const CalaosOsProtocol::UpdateStatus &status)> callback) override;
    void rebootMachine(std::function<void(const CalaosOsReply &reply)> callback) override;
    void getSystemInfo(std::function<void(bool success, const QJsonValue &res)> callback) override;
    void setProgressHandler(std::function<void(const QJsonObject &status)> handler) override;

    AdminRelay *relay() const { return m_relay; }

    static CalaosOsReply toReply(const AdminRelay::Outcome &outcome);

private:
    void call(const QString &action, const QJsonObject &params,
              std::function<void(const CalaosOsReply &reply)> callback);

    AdminRelay *m_relay;
    std::function<void(const QJsonObject &status)> m_progress;
};

#endif // ADMINRELAYBACKEND_H
