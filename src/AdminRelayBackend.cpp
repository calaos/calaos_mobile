#include "AdminRelayBackend.h"

using namespace CalaosOsProtocol;

AdminRelayBackend::AdminRelayBackend(AdminRelay *relay, QObject *parent):
    QObject(parent),
    m_relay(relay)
{
    connect(m_relay, &AdminRelay::upgradeProgress, this, [this](const QJsonObject &status)
    {
        if (m_progress)
            m_progress(status);
    });
}

CalaosOsReply AdminRelayBackend::toReply(const AdminRelay::Outcome &outcome)
{
    CalaosOsReply r;
    r.api = ApiVersion::V2;
    r.ok = outcome.ok;
    r.data = outcome.result;
    const QString &code = outcome.error.code;
    //an answer from calaos_server, whatever it says, is not a transport error
    const bool answered = outcome.ok ||
                          (code != AdminRelay::ErrTimeout && code != AdminRelay::ErrDisconnected &&
                           code != AdminRelay::ErrWebsocketRequired && code != AdminRelay::ErrTlsPending);
    r.httpStatus = answered ? 200 : 0;
    r.requestStarted = code != AdminRelay::ErrWebsocketRequired && code != AdminRelay::ErrTlsPending;
    if (!outcome.ok)
    {
        r.code = code;
        r.error = outcome.error.msg;
        r.retryAfter = outcome.error.retryAfter;
        r.upgradeInProgress = code == QLatin1String("upgrade_in_progress");
        r.packageNotFound = code == QLatin1String("package_not_found");
        if (r.upgradeInProgress && !outcome.error.jobId.isEmpty())
            r.data = QJsonObject{{ QStringLiteral("job_id"), outcome.error.jobId }};
    }
    return r;
}

void AdminRelayBackend::call(const QString &action, const QJsonObject &params,
                             std::function<void(const CalaosOsReply &reply)> callback)
{
    m_relay->request(action, params, [callback](const AdminRelay::Outcome &o)
    {
        if (callback)
            callback(toReply(o));
    });
}

void AdminRelayBackend::checkUpdates(std::function<void(const CalaosOsReply &reply)> callback)
{
    call(QStringLiteral("update_check"), QJsonObject(), callback);
}

void AdminRelayBackend::getAvailableUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback)
{
    call(QStringLiteral("update_available"), QJsonObject(), [callback](const CalaosOsReply &r)
    {
        if (callback)
            callback(r.ok, r.data.toObject(), r.error);
    });
}

void AdminRelayBackend::upgradePackage(const QString &package, std::function<void(const CalaosOsReply &reply)> callback)
{
    call(QStringLiteral("update_upgrade"), QJsonObject{{ QStringLiteral("package"), package }}, callback);
}

void AdminRelayBackend::upgradeAll(std::function<void(const CalaosOsReply &reply)> callback)
{
    call(QStringLiteral("update_upgrade_all"), QJsonObject(), callback);
}

void AdminRelayBackend::getUpdateStatus(std::function<void(bool ok, const UpdateStatus &status)> callback)
{
    call(QStringLiteral("update_status"), QJsonObject(), [callback](const CalaosOsReply &r)
    {
        if (callback)
            callback(r.ok, r.ok ? parseUpdateStatus(ApiVersion::V2, r.data) : UpdateStatus());
    });
}

void AdminRelayBackend::rebootMachine(std::function<void(const CalaosOsReply &reply)> callback)
{
    call(QStringLiteral("system_reboot"), QJsonObject(), callback);
}

void AdminRelayBackend::getSystemInfo(std::function<void(bool success, const QJsonValue &res)> callback)
{
    call(QStringLiteral("system_info"), QJsonObject(), [callback](const CalaosOsReply &r)
    {
        if (callback)
            callback(r.ok, r.data);
    });
}

void AdminRelayBackend::setProgressHandler(std::function<void(const QJsonObject &status)> handler)
{
    m_progress = handler;
}
