#ifndef UPDATEBACKEND_H
#define UPDATEBACKEND_H

#include <functional>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QString>
#include "CalaosOsProtocol.h"

//Outcome of one update call, whichever transport carried it: the
//calaos-container daemon directly (kiosk) or the admin relay of calaos_server
//(mobile).
struct CalaosOsReply
{
    bool ok = false;
    //0 on transport errors (connection severed/refused, no HTTP reply), which
    //are not server rejections: the daemon may be restarting
    int httpStatus = 0;
    CalaosOsProtocol::ApiVersion api = CalaosOsProtocol::ApiVersion::Unknown; //surface that answered
    QString error; //displayable message on failure
    //v2: the envelope data; legacy update/*: the whole document; legacy system/*: "output"
    QJsonValue data;
    bool upgradeInProgress = false;
    bool packageNotFound = false;
    //with httpStatus 0: why no HTTP answer came
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    bool requestStarted = true;
    //admin relay only: the error code of calaos_server's admin vocabulary
    QString code;
    //admin relay only: seconds to wait with "rate_limited", -1 otherwise
    int retryAfter = -1;

    //the request provably never reached the daemon (nothing can have started)
    bool neverReachedDaemon() const
    {
        return httpStatus == 0 &&
               (!requestStarted ||
                networkError == QNetworkReply::ConnectionRefusedError ||
                networkError == QNetworkReply::HostNotFoundError);
    }
};

//The update calls the update screens need, independent of the transport:
//CalaosOsAPI talks to the daemon on the kiosk, AdminRelayBackend relays them
//through calaos_server on mobile. Results use the daemon's /api/v2 shapes.
class UpdateBackend
{
public:
    virtual ~UpdateBackend() = default;

    //GET update/check: reply.data is the object keyed by package name
    virtual void checkUpdates(std::function<void(const CalaosOsReply &reply)> callback) = 0;
    //GET update/available: what the last check found, without a new one
    virtual void getAvailableUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback) = 0;
    //POST update/upgrade(-all): a <Job> in reply.data on /api/v2
    virtual void upgradePackage(const QString &package, std::function<void(const CalaosOsReply &reply)> callback) = 0;
    virtual void upgradeAll(std::function<void(const CalaosOsReply &reply)> callback) = 0;
    //GET update/status: progress plus the running or last job
    virtual void getUpdateStatus(std::function<void(bool ok, const CalaosOsProtocol::UpdateStatus &status)> callback) = 0;
    //reply.upgradeInProgress: refused because an upgrade runs
    virtual void rebootMachine(std::function<void(const CalaosOsReply &reply)> callback) = 0;
    //GET system/info: {hostname, uptime, cpu_usage, mem_usage}
    virtual void getSystemInfo(std::function<void(bool success, const QJsonValue &res)> callback) = 0;

    //Backends that push upgrade progress (<Status> objects) call it; the
    //kiosk reads the daemon's own event stream in UpdateManager instead.
    virtual void setProgressHandler(std::function<void(const QJsonObject &status)> handler)
    {
        Q_UNUSED(handler)
    }
};

#endif // UPDATEBACKEND_H
