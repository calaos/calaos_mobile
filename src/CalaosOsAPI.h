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

private:
    void checkErrors(const QJsonDocument &jdoc, NetworkRequest *n = nullptr);
    void doPost(QString apiPath, const QByteArray &postData, std::function<void(bool success)> callback);
    void doGet(QString apiPath, std::function<void(bool success, const QJsonValue &res)> callback);

    QNetworkAccessManager *netManager;

    QString calaosAddr, token;

    //errors of last api call if any
    QString lastError;
};

#endif // CALAOSOSAPI_H
