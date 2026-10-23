#include "CalaosOsAPI.h"
#include <QStringBuilder>
#include <QUrlQuery>
#include <memory>

using namespace CalaosOsProtocol;

#ifdef Q_OS_LINUX
#define TOKEN_FILE  "/run/calaos/calaos-ct.token"
#else
#define TOKEN_FILE  "C:/temp/calaos/calaos-ct.token"
#endif

CalaosOsAPI::CalaosOsAPI(QNetworkAccessManager *nm, QObject *parent):
    QObject(parent),
    netManager(nm)
{
    if (!netManager)
        netManager = new QNetworkAccessManager(this);

    //read on first use: tests and tools point at another file first
    tokenFile = QStringLiteral(TOKEN_FILE);

    calaosAddr = "http://127.0.0.1:8000";
}

CalaosOsAPI::~CalaosOsAPI()
{
    //requests abort their reply when deleted: do it while the manager owning
    //the replies still exists, and without calling back anyone
    const auto requests = findChildren<NetworkRequest *>(Qt::FindDirectChildrenOnly);
    for (NetworkRequest *n: requests)
    {
        disconnect(n, nullptr, this, nullptr);
        delete n;
    }
}

void CalaosOsAPI::setTokenFile(const QString &path)
{
    tokenFile = path;
    tokenReadable = true;
    tokenLoaded = false;
}

void CalaosOsAPI::ensureToken()
{
    if (!tokenLoaded)
        reloadToken();
}

void CalaosOsAPI::reloadToken()
{
    tokenLoaded = true;
    QFile file(tokenFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        token = QString::fromUtf8(file.readAll().trimmed());
        tokenReadable = true;
        return;
    }

    //re-read on every reconnect: only warn once per outage
    if (tokenReadable)
        qWarning() << "unable to read " << tokenFile;
    tokenReadable = false;
    token.clear();
}

void CalaosOsAPI::rebootMachine(std::function<void (const CalaosOsReply &)> callback)
{
    Route r;
    r.method = NetworkRequest::HttpPost;
    r.v2Path = QStringLiteral("/system/reboot");
    r.legacyPath = QStringLiteral("/api/system/reboot");
    r.legacyShape = LegacyBody::Output;
    call(r, callback);
}

void CalaosOsAPI::restartApp(std::function<void (bool)> callback)
{
    Route r;
    r.method = NetworkRequest::HttpPost;
    r.v2Path = QStringLiteral("/system/restart");
    r.legacyPath = QStringLiteral("/api/system/restart");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok);
    });
}

void CalaosOsAPI::getFsStatus(std::function<void (bool, const QJsonValue &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/system/fs_status");
    r.legacyPath = QStringLiteral("/api/system/fs_status");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok, reply.ok ? reply.data : QJsonValue());
    });
}

void CalaosOsAPI::rollbackSnapshot(std::function<void (bool)> callback)
{
    Route r;
    r.method = NetworkRequest::HttpPost;
    r.v2Path = QStringLiteral("/system/rollback_snapshot");
    r.legacyPath = QStringLiteral("/api/system/rollback_snapshot");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok);
    });
}

void CalaosOsAPI::listInstallDevices(std::function<void (bool, const QJsonValue &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/system/install/list_devices");
    r.legacyPath = QStringLiteral("/api/system/install/list_devices");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok, reply.ok ? reply.data : QJsonValue());
    });
}

//The installer stays on the legacy routes: it runs from a live image whose
//daemon version is fixed, and the v2 install/status has another shape.
void CalaosOsAPI::startInstallation(QString device, std::function<void (bool)> callbackFinished, std::function<void (QString)> callbackStdout)
{
    ensureToken();
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new AsyncJob([this, device, callbackStdout](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % "/api/system/install/start";

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpPost, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", CalaosOsProtocol::authorizationHeader(token));
                                  n->setCustomHeader("Content-Type", "application/json");
                                  n->setResultType(NetworkRequest::TypeRawData);

                                  QJsonObject d = {{ "device", device }};
                                  QJsonDocument doc(d);
                                  n->setPostData(doc.toJson(QJsonDocument::Compact));

                                  connect(n, &NetworkRequest::dataReadyRead, this,
                                          [callbackStdout](const QByteArray &data)
                                          {
                                              callbackStdout(QString::fromUtf8(data));
                                          });

                                  connect(n, &NetworkRequest::finishedData, this,
                                          [n, job](int success, const QByteArray &)
                                          {
                                              n->deleteLater();

                                              if (success == NetworkRequest::RequestSuccess)
                                                  job->emitSuccess();
                                              else
                                                  job->emitFailed();
                                          });

                                  if (!n->start())
                                  {
                                      delete n;
                                      lastError = tr("Failed to start network request");
                                      job->emitFailed();
                                  }
                              }));

    jobs->append(new AsyncJob([this, device, callbackStdout](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % "/api/system/install/status";

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpGet, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", CalaosOsProtocol::authorizationHeader(token));
                                  n->setCustomHeader("Content-Type", "application/json");
                                  n->setResultType(NetworkRequest::TypeJson);

                                  connect(n, &NetworkRequest::finishedJson, this,
                                          [this, n, job](int success, const QJsonDocument &jdoc)
                                          {
                                              n->deleteLater();

                                              if (success == NetworkRequest::RequestSuccess)
                                              {
                                                  QJsonObject jobj = jdoc.object();
                                                  if (jobj["error"].toBool())
                                                  {
                                                      checkErrors(jdoc, n);
                                                      job->emitFailed();
                                                  }
                                                  else
                                                  {
                                                      auto code = jobj["exit_code"].toInt();

                                                      if (code != 0)
                                                      {
                                                          auto e = QStringLiteral("Installation failed with exit code:%1").arg(code);
                                                          lastError.append(e);
                                                          job->emitFailed();
                                                      }

                                                      job->emitSuccess();
                                                  }
                                              }
                                              else
                                              {
                                                  checkErrors(jdoc, n);
                                                  job->emitFailed();
                                              }
                                          });

                                  if (!n->start())
                                  {
                                      delete n;
                                      lastError = tr("Failed to start network request");
                                      job->emitFailed();
                                  }
                              }));

    connect(jobs, &AsyncJobs::failed, this, [callbackFinished](AsyncJob *)
            {
                callbackFinished(false);
            });

    connect(jobs, &AsyncJobs::finished, this, [callbackFinished](const QVariant &)
            {
                callbackFinished(true);
            });

    jobs->start();
}

void CalaosOsAPI::getNetworkInterfaces(std::function<void (bool, const QJsonValue &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/network/list");
    r.legacyPath = QStringLiteral("/api/network/list");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok, reply.ok ? reply.data : QJsonValue());
    });
}

void CalaosOsAPI::getSystemInfo(std::function<void (bool, const QJsonValue &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/system/info");
    r.legacyPath = QStringLiteral("/api/system/info");
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok, reply.ok ? reply.data : QJsonValue());
    });
}

void CalaosOsAPI::configureNetwork(const QJsonObject &networkConfig, std::function<void (bool)> callback)
{
    QByteArray data = QJsonDocument(networkConfig).toJson(QJsonDocument::Compact);
    qDebug() << "Configure network:" << QString(data);

    const QString intf = networkConfig["name"].toString();
    Route r;
    r.method = NetworkRequest::HttpPost;
    r.v2Path = QStringLiteral("/network/%1").arg(intf);
    r.v2Body = data;
    r.legacyPath = QStringLiteral("/api/network/%1").arg(intf);
    r.legacyBody = data;
    r.legacyShape = LegacyBody::Output;
    call(r, [callback](const CalaosOsReply &reply)
    {
        if (callback)
            callback(reply.ok);
    });
}

void CalaosOsAPI::checkUpdates(std::function<void (const CalaosOsReply &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/update/check");
    r.legacyPath = QStringLiteral("/api/update/check");
    call(r, callback);
}

void CalaosOsAPI::getAvailableUpdates(std::function<void (bool, const QJsonObject &, const QString &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/update/available");
    r.legacyPath = QStringLiteral("/api/update/available");
    call(r, [callback](const CalaosOsReply &reply)
    {
        callback(reply.ok, reply.data.toObject(), reply.error);
    });
}

void CalaosOsAPI::getInstalledPackages(std::function<void (bool, int, const QJsonObject &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/update/installed");
    r.legacyPath = QStringLiteral("/api/update/installed");
    call(r, [callback](const CalaosOsReply &reply)
    {
        callback(reply.ok, reply.httpStatus, reply.data.toObject());
    });
}

void CalaosOsAPI::upgradePackage(const QString &package, std::function<void (const CalaosOsReply &)> callback)
{
    QUrlQuery query;
    query.addQueryItem("package", package);

    Route r;
    r.method = NetworkRequest::HttpPost;
    r.upgradeRoute = true;
    r.v2Path = QStringLiteral("/update/upgrade");
    r.v2Body = QJsonDocument(QJsonObject{{ "package", package }}).toJson(QJsonDocument::Compact);
    r.legacyPath = QStringLiteral("/api/update/upgrade?") + query.toString(QUrl::FullyEncoded);
    call(r, callback);
}

void CalaosOsAPI::upgradeAll(std::function<void (const CalaosOsReply &)> callback)
{
    Route r;
    r.method = NetworkRequest::HttpPost;
    r.upgradeRoute = true;
    r.v2Path = QStringLiteral("/update/upgrade-all");
    r.legacyPath = QStringLiteral("/api/update/upgrade-all");
    call(r, callback);
}

void CalaosOsAPI::getUpdateStatus(std::function<void (bool, const UpdateStatus &)> callback)
{
    Route r;
    r.v2Path = QStringLiteral("/update/status");
    r.legacyPath = QStringLiteral("/api/update/status");
    call(r, [callback](const CalaosOsReply &reply)
    {
        callback(reply.ok, parseUpdateStatus(reply.api, reply.data));
    });
}

void CalaosOsAPI::detectApiVersion(std::function<void ()> callback)
{
    if (m_apiVersion != ApiVersion::Unknown)
    {
        callback();
        return;
    }

    //any v2 route tells; update/status is cheap and allowed to every bearer
    Route r;
    r.v2Path = QStringLiteral("/update/status");
    call(r, [callback](const CalaosOsReply &)
    {
        callback();
    });
}

QNetworkRequest CalaosOsAPI::eventsRequest()
{
    ensureToken();

    QString addr = calaosAddr;
    if (addr.startsWith(QStringLiteral("https://")))
        addr.replace(0, 8, QStringLiteral("wss://"));
    else if (addr.startsWith(QStringLiteral("http://")))
        addr.replace(0, 7, QStringLiteral("ws://"));

    //the token goes in the header only: a URL ends up in logs
    const QString path = m_apiVersion == ApiVersion::Legacy ?
                             QStringLiteral("/api/events") :
                             QStringLiteral("/api/v2/events");
    QNetworkRequest req(QUrl(addr + path));
    req.setRawHeader("Authorization", authorizationHeader(token).toUtf8());
    return req;
}

void CalaosOsAPI::checkErrors(const QJsonDocument &jdoc, NetworkRequest *n)
{
    QJsonObject jobj = jdoc.object();
    if (jobj["error"].toBool())
    {
        lastError.append(jobj["msg"].toString());
    }

    if (n)
        lastError.append(n->getLastError());
}

void CalaosOsAPI::call(const Route &route, std::function<void (const CalaosOsReply &)> callback)
{
    const bool useV2 = !route.v2Path.isEmpty() &&
                       (m_apiVersion != ApiVersion::Legacy || route.legacyPath.isEmpty());
    send(route, useV2, false, callback);
}

void CalaosOsAPI::send(const Route &route, bool useV2, bool authRetried, std::function<void (const CalaosOsReply &)> callback)
{
    ensureToken();
    const QString url = calaosAddr % (useV2 ? QStringLiteral("/api/v2") % route.v2Path : route.legacyPath);

    NetworkRequest *n = new NetworkRequest(url, route.method, this);
    n->setNetManager(netManager);
    n->setCustomHeader("Authorization", authorizationHeader(token));
    if (route.method != NetworkRequest::HttpGet)
        n->setCustomHeader("Content-Type", "application/json");
    n->setResultType(NetworkRequest::TypeJson);
    n->setPostData(useV2 ? route.v2Body : route.legacyBody);

    connect(n, &NetworkRequest::finishedJson, this,
            [this, n, route, useV2, authRetried, callback](int success, const QJsonDocument &doc)
    {
        const int httpStatus = n->getHttpStatusCode();
        const QString netError = n->getLastError();
        const QNetworkReply::NetworkError networkError = n->getNetworkError();
        n->deleteLater();

        //the token may have been rewritten since we read it: re-read, retry once
        if (httpStatus == 401 && !authRetried)
        {
            reloadToken();
            send(route, useV2, true, callback);
            return;
        }

        //no HTTP answer: the daemon may be restarting into another version
        if (httpStatus == 0)
            m_apiVersion = ApiVersion::Unknown;

        if (useV2 && httpStatus != 0)
        {
            const Envelope env = parseEnvelope(doc);
            if (isLegacyDaemonReply(httpStatus, doc))
            {
                m_apiVersion = ApiVersion::Legacy;
                if (!route.legacyPath.isEmpty())
                {
                    send(route, false, false, callback);
                    return;
                }
            }
            else if (env.isEnvelope)
            {
                //only an envelope proves /api/v2: an old daemon's 401 is not one
                m_apiVersion = ApiVersion::V2;
                if (route.upgradeRoute && isUpgradeRouteMissing(httpStatus, env) &&
                    !route.legacyPath.isEmpty())
                {
                    send(route, false, false, callback);
                    return;
                }
            }
        }

        CalaosOsReply reply = useV2 ?
                                  v2Reply(success, httpStatus, doc, netError) :
                                  legacyReply(route, success, httpStatus, doc, netError);
        reply.networkError = networkError;
        if (callback)
            callback(reply);
    });

    if (!n->start())
    {
        delete n;
        CalaosOsReply reply;
        reply.api = useV2 ? ApiVersion::V2 : ApiVersion::Legacy;
        reply.requestStarted = false;
        reply.error = tr("Failed to start network request");
        if (callback)
            callback(reply);
    }
}

CalaosOsReply CalaosOsAPI::v2Reply(int success, int httpStatus, const QJsonDocument &doc, const QString &netError) const
{
    CalaosOsReply reply;
    reply.api = ApiVersion::V2;
    reply.httpStatus = httpStatus;

    const Envelope env = parseEnvelope(doc);
    reply.code = env.code;
    reply.data = env.data;
    reply.ok = success == NetworkRequest::RequestSuccess && env.isEnvelope && !env.error;
    if (!reply.ok)
    {
        if (!env.msg.isEmpty())
            reply.error = env.msg;
        else if (!netError.isEmpty())
            reply.error = netError;
        else
            reply.error = tr("Invalid answer from the update service");
    }

    reply.upgradeInProgress = isUpgradeInProgress(ApiVersion::V2, httpStatus, env, QString());
    reply.packageNotFound = isPackageNotFound(ApiVersion::V2, httpStatus, env, QString());
    return reply;
}

CalaosOsReply CalaosOsAPI::legacyReply(const Route &route, int success, int httpStatus, const QJsonDocument &doc, const QString &netError) const
{
    CalaosOsReply reply;
    reply.api = ApiVersion::Legacy;
    reply.httpStatus = httpStatus;

    const QJsonObject jobj = doc.object();
    if (success == NetworkRequest::RequestSuccess)
    {
        if (route.legacyShape == LegacyBody::Output)
        {
            reply.ok = !jobj["error"].toBool();
            reply.data = jobj["output"];
            if (!reply.ok)
                reply.error = jobj["msg"].toString();
        }
        else
        {
            reply.ok = true;
            reply.data = jobj;
        }
    }
    else
    {
        reply.error = jobj["error"].toBool() ? jobj["msg"].toString() : netError;
    }

    if (!reply.ok)
    {
        reply.upgradeInProgress = isUpgradeInProgress(ApiVersion::Legacy, httpStatus, Envelope(), reply.error);
        reply.packageNotFound = isPackageNotFound(ApiVersion::Legacy, httpStatus, Envelope(), reply.error);
    }
    return reply;
}
