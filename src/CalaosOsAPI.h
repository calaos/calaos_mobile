#ifndef CALAOSOSAPI_H
#define CALAOSOSAPI_H

#include <QObject>
#include "NetworkRequest.h"
#include "AsyncJobs.h"

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

    void rebootMachine(std::function<void(bool success)> callback);
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

    //check for available updates on the calaos-container update daemon (GET /api/update/check). Blocking server-side, can take tens of seconds.
    void checkUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback);
    //get updates already found by a previous check, without triggering a new one (GET /api/update/available). Instant.
    void getAvailableUpdates(std::function<void(bool ok, const QJsonObject &pkgs, const QString &err)> callback);
    //get currently installed packages (GET /api/update/installed). May 404 on older daemons, hence the httpStatus.
    void getInstalledPackages(std::function<void(bool ok, int httpStatus, const QJsonObject &pkgs)> callback);
    //upgrade a single package by name (POST /api/update/upgrade?package=...). Blocking server-side, can take minutes.
    //httpStatus is 0 on transport errors (connection severed/refused, no HTTP reply), which is not a server rejection:
    //the transaction may still be running. Authoritative rejections come as HTTP errors (500 + {"error","msg"}).
    void upgradePackage(const QString &package, std::function<void(bool ok, int httpStatus, const QString &err)> callback);
    //upgrade all packages (POST /api/update/upgrade-all). Blocking server-side, can take minutes.
    //httpStatus is 0 on transport errors: upgrade-all installs calaos-container first, which restarts the daemon
    //and severs this POST while the transaction goes on, so a transport error is not an upgrade failure.
    void upgradeAll(std::function<void(bool ok, int httpStatus, const QString &err)> callback);
    //get the progress/status of an ongoing (or last) upgrade (GET /api/update/status).
    void getUpdateStatus(std::function<void(bool ok, const QJsonObject &status)> callback);

    //auth token used for calaos-container requests
    QString getToken() const;
    //base address of the calaos-container daemon
    QString getBaseAddr() const;

private:
    void checkErrors(const QJsonDocument &jdoc, NetworkRequest *n = nullptr);
    void doPost(QString apiPath, const QByteArray &postData, std::function<void(bool success)> callback);
    void doGet(QString apiPath, std::function<void(bool success, const QJsonValue &res)> callback);
    //same as doPost/doGet but without the {"error","msg","output"} envelope unwrap used by system/* endpoints:
    //passes the full document and HTTP status to the callback, as used by the update/* endpoints.
    void doPostRaw(QString apiPath, const QByteArray &postData, std::function<void(bool success, int httpStatus, const QJsonDocument &doc)> callback);
    void doGetRaw(QString apiPath, std::function<void(bool success, int httpStatus, const QJsonDocument &doc)> callback);

    QNetworkAccessManager *netManager;

    QString calaosAddr, token;

    //errors of last api call if any
    QString lastError;
};

#endif // CALAOSOSAPI_H
