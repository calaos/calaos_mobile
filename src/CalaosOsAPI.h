#ifndef CALAOSOSAPI_H
#define CALAOSOSAPI_H

#include <QObject>
#include "NetworkRequest.h"
#include "AsyncJobs.h"
#include "CalaosOsProtocol.h"

//Outcome of one call to the calaos-container daemon, whichever surface answered.
struct CalaosOsReply
{
    bool ok = false;
    //0 on transport errors (connection severed/refused, no HTTP reply), which
    //are not server rejections: the daemon may be restarting
    int httpStatus = 0;
    CalaosOsProtocol::ApiVersion api = CalaosOsProtocol::ApiVersion::Unknown; //surface that answered
    QString code;  //v2 error code, empty on the legacy surface
    QString error; //displayable message on failure
    //v2: the envelope data; legacy update/*: the whole document; legacy system/*: "output"
    QJsonValue data;
    bool upgradeInProgress = false;
    bool packageNotFound = false;
    //with httpStatus 0: why no HTTP answer came
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    bool requestStarted = true;

    //the request provably never reached the daemon (nothing can have started)
    bool neverReachedDaemon() const
    {
        return httpStatus == 0 &&
               (!requestStarted ||
                networkError == QNetworkReply::ConnectionRefusedError ||
                networkError == QNetworkReply::HostNotFoundError);
    }
};

class CalaosOsAPI: public QObject
{
    Q_OBJECT
public:
    static CalaosOsAPI *Instance()
    {
        static CalaosOsAPI api;
        return &api;
    }

    CalaosOsAPI(QNetworkAccessManager *nm = nullptr, QObject *parent = nullptr);
    virtual ~CalaosOsAPI();

    //reply.upgradeInProgress: refused because an upgrade runs (409)
    void rebootMachine(std::function<void(const CalaosOsReply &reply)> callback);
    void restartApp(std::function<void(bool success)> callback);
    void getFsStatus(std::function<void(bool success, const QJsonValue &res)> callback);

    void rollbackSnapshot(std::function<void(bool success)> callback);

    void listInstallDevices(std::function<void(bool success, const QJsonValue &res)> callback);
    void startInstallation(QString device,
        std::function<void(bool success)> callbackFinished,
        std::function<void(QString out)> callbackStdout);

    void getNetworkInterfaces(std::function<void(bool success, const QJsonValue &res)> callback);
    void getSystemInfo(std::function<void(bool success, const QJsonValue &res)> callback);

    void configureNetwork(const QJsonObject &networkConfig, std::function<void(bool success)> callback);

    //check for available updates (GET update/check). Blocking server-side, can take tens of seconds.
    //reply.data is the object keyed by package name.
    void checkUpdates(std::function<void(const CalaosOsReply &reply)> callback);
    //get updates already found by a previous check, without triggering a new one (GET update/available). Instant.
    void getAvailableUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback);
    //get currently installed packages (GET update/installed). May 404 on older daemons, hence the httpStatus.
    void getInstalledPackages(std::function<void(bool ok, int httpStatus, const QJsonObject &pkgs)> callback);
    //upgrade a package / all packages: 202 + job in reply.data on /api/v2, synchronous on legacy,
    //where a transport error is not a failure (a self-upgrading daemon severs the POST)
    void upgradePackage(const QString &package, std::function<void(const CalaosOsReply &reply)> callback);
    void upgradeAll(std::function<void(const CalaosOsReply &reply)> callback);
    //progress of an ongoing (or last) upgrade, plus the job on /api/v2 (GET update/status)
    void getUpdateStatus(std::function<void(bool ok, const CalaosOsProtocol::UpdateStatus &status)> callback);

    //WebSocket handshake of the daemon event stream, for the detected API version
    QNetworkRequest eventsRequest();

    CalaosOsProtocol::ApiVersion apiVersion() const { return m_apiVersion; }
    //forget the detected API version: the daemon may have been replaced by another version
    void resetApiVersion() { m_apiVersion = CalaosOsProtocol::ApiVersion::Unknown; }
    //find out the API version if unknown, then call back
    void detectApiVersion(std::function<void()> callback);

    //re-read the token file, e.g. after a 401 or before reconnecting
    void reloadToken();

    //for tests: point at a fake daemon and token file
    void setBaseAddr(const QString &addr) { calaosAddr = addr; }
    void setTokenFile(const QString &path);

private:
    enum class LegacyBody
    {
        Raw,    //update/*: the document itself
        Output, //system/*, network/*: {"error","msg","output"}
    };

    struct Route
    {
        NetworkRequest::HttpType method = NetworkRequest::HttpGet;
        QString v2Path;     //relative to /api/v2
        QByteArray v2Body;
        QString legacyPath; //full path, empty when the call has no legacy route
        QByteArray legacyBody;
        LegacyBody legacyShape = LegacyBody::Raw;
        bool upgradeRoute = false;
    };

    void call(const Route &route, std::function<void(const CalaosOsReply &)> callback);
    void send(const Route &route, bool useV2, bool authRetried, std::function<void(const CalaosOsReply &)> callback);
    CalaosOsReply v2Reply(int success, int httpStatus, const QJsonDocument &doc, const QString &netError) const;
    CalaosOsReply legacyReply(const Route &route, int success, int httpStatus, const QJsonDocument &doc, const QString &netError) const;

    void checkErrors(const QJsonDocument &jdoc, NetworkRequest *n = nullptr);

    QNetworkAccessManager *netManager;

    void ensureToken();

    QString calaosAddr, token, tokenFile;
    bool tokenReadable = true;
    bool tokenLoaded = false;

    CalaosOsProtocol::ApiVersion m_apiVersion = CalaosOsProtocol::ApiVersion::Unknown;

    //errors of last install call if any
    QString lastError;
};

#endif // CALAOSOSAPI_H
