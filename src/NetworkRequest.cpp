#include "NetworkRequest.h"
#include "TlsPinning.h"
#include "LogRedact.h"
#include "NetworkLimits.h"
#include "LogCategories.h"

//The kiosk polls the daemon every few seconds: request traces stay off the
//journal unless asked for with QT_LOGGING_RULES="calaos.net.debug=true".
//Errors are warnings and always shown.

//Abort a transfer that stops making progress. Set on the request and not on
//the manager because setNetManager() can hand us a manager owned by someone
//else (CalaosOsAPI), which we must not reconfigure behind its back.
static const int HTTP_TRANSFER_TIMEOUT = 30000;

NetworkRequest::NetworkRequest(QObject *parent):
    QObject(parent),
    sslConfiguration(QSslConfiguration::defaultConfiguration())
{
}

NetworkRequest::NetworkRequest(QString u, HttpType t, QObject *parent):
    QObject(parent),
    url(u),
    type(t),
    sslConfiguration(QSslConfiguration::defaultConfiguration())
{
}

NetworkRequest::~NetworkRequest()
{
    cancel();
    if (delNm)
        delete netmanager;
}

void NetworkRequest::setNetManager(QNetworkAccessManager *netManager)
{
    netmanager = netManager;
}

void NetworkRequest::setCustomHeader(QString header, QString value)
{
    headers[header] = value;
}

void NetworkRequest::setCertificate(QString path)
{
    cacerts = QSslCertificate::fromPath(path, QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
    sslConfiguration.addCaCertificates(cacerts);
}

bool NetworkRequest::start()
{
    if (reply)
    {
        qCDebug(lcNet) << "Request already in progress";
        return false;
    }

    if (!netmanager)
    {
        delNm = true;
        netmanager = new QNetworkAccessManager(this);
    }

    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setTransferTimeout(HTTP_TRANSFER_TIMEOUT);
    //Per request, the manager may be shared: credentials or tokens must not
    //follow a redirect to another host.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    lastError.clear();
    request.setSslConfiguration(sslConfiguration);

    qCDebug(lcNet) << "New request to" << LogRedact::redactUrl(QUrl(url));

    //set ssl config
    QSslConfiguration sslConfig(QSslConfiguration::defaultConfiguration());
    sslConfig.setCaCertificates(cacerts);
    request.setSslConfiguration(sslConfig);

    auto it = headers.constBegin();
    while (it != headers.constEnd())
    {
        request.setRawHeader(it.key().toLocal8Bit(), it.value().toLocal8Bit());
        ++it;
    }

    if (type == HttpGet)
        reply = netmanager->get(request);
    else if (type == HttpPost)
        reply = netmanager->post(request, postData);
    else if (type == HttpPut)
        reply = netmanager->put(request, postData);
    else if (type == HttpDelete)
        reply = netmanager->deleteResource(request);
    else if (type == HttpHead)
        reply = netmanager->head(request);

    if (!reply)
    {
        qCWarning(lcNet) << "Failed to create request";
        return false;
    }

    //A kept-alive channel was validated against the old pins; the manager
    //may be shared, hence unique.
    connect(TlsPinStore::instance(), &TlsPinStore::pinsChanged,
            netmanager, &QNetworkAccessManager::clearConnectionCache, Qt::UniqueConnection);
    connect(reply, &QNetworkReply::sslErrors, this, &NetworkRequest::nmSslErrors);
    connect(reply, &QNetworkReply::encrypted, this, [this]()
    {
        TlsPinning::keyMatchesPin(reply);
    });

    dataRead.clear();

    if (resType == TypeJson)
        NetworkLimits::limitDownload(reply, NetworkLimits::MaxJsonBytes);
    else if (resType == TypeRawData)
        NetworkLimits::limitDownload(reply, NetworkLimits::MaxPictureBytes);
    else
        NetworkLimits::limitDownload(reply, maxFileBytes);

    if (resType == TypeJson)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinishedJson);
        connect(reply, &QNetworkReply::readyRead, this, [this]()
                {
                    //The bound's abort() runs finished synchronously: reply is already null.
                    if (!reply || reply->error() == QNetworkReply::OperationCanceledError)
                        return;
                    auto d = reply->readAll();
                    emit dataReadyRead(d);
                    dataRead.append(d);
                });
    }
    else if (resType == TypeRawData)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinishedData);
        connect(reply, &QNetworkReply::readyRead, this, [this]()
                {
                    //The bound's abort() runs finished synchronously: reply is already null.
                    if (!reply || reply->error() == QNetworkReply::OperationCanceledError)
                        return;
                    auto d = reply->readAll();
                    emit dataReadyRead(d);
                    dataRead.append(d);
                });
    }
    else if (resType == TypeFile)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinished);
        connect(reply, &QNetworkReply::readyRead,
                this, &NetworkRequest::nmReadyRead);
        if (!dlFile)
        {
            lastError = "dlFile is invalid!, aborting request.";
            qWarning() << "dlFile is invalid!, aborting request.";
            reply->abort();
            return false;
        }
        if (!dlFile->isOpen())
        {
            if (!dlFile->open(QFile::WriteOnly | QIODevice::Truncate))
                qWarning() << "Error opening file '" << dlFile->fileName() << "' for write";
        }
    }

    return true;
}

void NetworkRequest::cancel()
{
    if (reply)
        reply->abort();
}

void NetworkRequest::nmReadyRead()
{
    if (!reply || reply->error() == QNetworkReply::OperationCanceledError)
        return;
    auto d = reply->readAll();
    dlFile->write(d);
    emit dataReadyRead(d);
}

void NetworkRequest::nmFinished()
{
    httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    networkError = reply->error();

    if (reply->error() != QNetworkReply::NoError)
    {
        lastError = LogRedact::redactError(reply->errorString(), reply->url());
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        emit finished(RequestHttpError);

        reply->deleteLater();
        reply = nullptr;

        return;
    }

    if (reply->bytesAvailable() > 0)
        dlFile->write(reply->readAll());
    dlFile->close();

    reply->deleteLater();
    reply = nullptr;

    emit finished(RequestSuccess);
}

void NetworkRequest::nmFinishedJson()
{
    QJsonParseError err;
    QJsonDocument jdoc;

    httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    networkError = reply->error();

    if (reply->error() != QNetworkReply::NoError)
    {
        lastError = LogRedact::redactError(reply->errorString(), reply->url());
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        jdoc = QJsonDocument::fromJson(dataRead, &err);
        emit finishedJson(RequestHttpError, jdoc);

        reply->deleteLater();
        reply = nullptr;

        return;
    }

    jdoc = QJsonDocument::fromJson(dataRead, &err);

    if (dataRead.isEmpty())
    {
        //do not treat empty result as error
        err.error = QJsonParseError::NoError;
    }

    if (err.error != QJsonParseError::NoError)
    {
        lastError = "JSON parse error " + err.errorString() + " at offset: " + QString::number(err.offset);
        //Not the body itself: it may hold a secret and free text cannot be masked.
        qCWarning(lcNet) << "JSON parse error " << err.errorString() << " at offset: " << err.offset
                         << "(body of" << dataRead.size() << "bytes)";
    }
    else
        qCDebug(lcNet) << "Response status" << httpStatusCode << "," << dataRead.size() << "bytes";

    reply->deleteLater();
    reply = nullptr;

    emit finishedJson(err.error == QJsonParseError::NoError?RequestSuccess:RequestJsonParseError, jdoc);
}

void NetworkRequest::nmFinishedData()
{
    httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    networkError = reply->error();

    if (reply->error() != QNetworkReply::NoError)
    {
        lastError = LogRedact::redactError(reply->errorString(), reply->url());
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        emit finishedData(RequestHttpError, QByteArray());

        reply->deleteLater();
        reply = nullptr;

        return;
    }

    reply->deleteLater();
    emit finishedData(RequestSuccess, dataRead);

    reply = nullptr;
}

void NetworkRequest::nmSslErrors(const QList<QSslError> &errors)
{
    qWarning() << "SSL errors:";
    for (const QSslError &e: errors)
    {
        qWarning() << e.errorString();
    }

    //No one to ask from here: a certificate the user did not pin through the
    //Calaos connection fails the request.
    TlsPinning::acceptIfPinned(qobject_cast<QNetworkReply *>(sender()), errors);
}
