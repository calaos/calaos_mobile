#include "CalaosOsAPI.h"
#include <QStringBuilder>

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

    QFile file(TOKEN_FILE);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        token = file.readAll().trimmed();
    else
        qWarning() << "unable to read " << TOKEN_FILE;

    calaosAddr = "http://127.0.0.1:8000";
}

CalaosOsAPI::~CalaosOsAPI()
{
}

void CalaosOsAPI::rebootMachine(std::function<void (bool)> callback)
{
    doPost("/api/system/reboot", {}, callback);
}

void CalaosOsAPI::restartApp(std::function<void (bool)> callback)
{
    doPost("/api/system/restart", {}, callback);
}

void CalaosOsAPI::getFsStatus(std::function<void (bool, const QJsonValue &)> callback)
{
    doGet("/api/system/fs_status", callback);
}

void CalaosOsAPI::rollbackSnapshot(std::function<void (bool)> callback)
{
    doPost("/api/system/rollback_snapshot", {}, callback);
}

void CalaosOsAPI::listInstallDevices(std::function<void (bool, const QJsonValue &)> callback)
{
    doGet("/api/system/install/list_devices", callback);
}

void CalaosOsAPI::startInstallation(QString device, std::function<void (bool)> callbackFinished, std::function<void (QString)> callbackStdout)
{
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new AsyncJob([this, device, callbackStdout](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % "/api/system/install/start";

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpPost, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", QStringLiteral("bearer %1").arg(token));
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
                                      lastError = "Failed to start network request";
                                      job->emitFailed();
                                  }
                              }));

    jobs->append(new AsyncJob([this, device, callbackStdout](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % "/api/system/install/status";

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpGet, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", QStringLiteral("bearer %1").arg(token));
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
                                      lastError = "Failed to start network request";
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
    doGet("/api/network/list", callback);
}

void CalaosOsAPI::getSystemInfo(std::function<void (bool, const QJsonValue &)> callback)
{
    doGet("/api/system/info", callback);
}

void CalaosOsAPI::configureNetwork(const QJsonObject &networkConfig, std::function<void (bool)> callback)
{
    QByteArray data = QJsonDocument(networkConfig).toJson(QJsonDocument::Compact);
    qDebug() << "Configure network:" << QString(data);
    doPost(QStringLiteral("/api/network/%1").arg(networkConfig["name"].toString()), data, callback);
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

void CalaosOsAPI::doPost(QString apiPath, const QByteArray &postData, std::function<void (bool)> callback)
{
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new AsyncJob([this, apiPath, postData](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % apiPath;

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpPost, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", QStringLiteral("bearer %1").arg(token));
                                  n->setCustomHeader("Content-Type", "application/json");
                                  n->setResultType(NetworkRequest::TypeJson);
                                  n->setPostData(postData);

                                  connect(n, &NetworkRequest::finishedJson, this, [this, n, job](int success, const QJsonDocument &jdoc)
                                          {
                                              n->deleteLater();

                                              if (success == NetworkRequest::RequestSuccess)
                                              {
                                                  job->emitSuccess();
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
                                      lastError = "Failed to start network request";
                                      job->emitFailed();
                                  }
                              }));

    connect(jobs, &AsyncJobs::failed, this, [callback](AsyncJob *)
            {
                callback(false);
            });

    connect(jobs, &AsyncJobs::finished, this, [callback](const QVariant &)
            {
                callback(true);
            });

    jobs->start();
}

void CalaosOsAPI::doGet(QString apiPath, std::function<void (bool, const QJsonValue &)> callback)
{
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new AsyncJob([this, apiPath](AsyncJob *job, const QVariant &)
                              {
                                  lastError.clear();
                                  QString url = calaosAddr % apiPath;

                                  NetworkRequest *n = new NetworkRequest(url, NetworkRequest::HttpGet, this);
                                  n->setNetManager(netManager);
                                  n->setCustomHeader("Authorization", QStringLiteral("bearer %1").arg(token));
                                  n->setResultType(NetworkRequest::TypeJson);

                                  connect(n, &NetworkRequest::finishedJson, this, [this, n, job](int success, const QJsonDocument &jdoc)
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
                                                      QJsonValue v = jobj["output"];
                                                      job->emitSuccess(v);
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
                                      lastError = "Failed to start network request";
                                      job->emitFailed();
                                  }
                              }));

    connect(jobs, &AsyncJobs::failed, this, [callback](AsyncJob *)
            {
                callback(false, {});
            });

    connect(jobs, &AsyncJobs::finished, this, [callback](const QVariant &data)
            {
                callback(true, data.toJsonValue());
            });

    jobs->start();
}

