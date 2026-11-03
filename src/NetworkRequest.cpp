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
    failure.clear();
    bound = bodyLimit();
    request.setSslConfiguration(sslConfiguration);

    //Checked before anything is sent: a failure here is reported by the return
    //value only, never by a signal as well.
    if (resType == TypeFile)
    {
        if (!dlFile)
        {
            lastError = QStringLiteral("dlFile is invalid!, aborting request.");
            qCWarning(lcNet) << lastError;
            return false;
        }
        const bool wasOpen = dlFile->isOpen();
        if (!wasOpen && !dlFile->open(QFile::WriteOnly | QIODevice::Truncate))
        {
            lastError = QStringLiteral("Cannot open the download file for write: ") + dlFile->errorString();
            qCWarning(lcNet) << "Error opening file '" << dlFile->fileName() << "' for write";
            return false;
        }
        //Only a file opened here may be deleted: the caller's open file, or a
        //content:// URI, stays theirs.
        ownsOpen = !wasOpen;
    }

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
    written = 0;

    //No setReadBufferSize(bound + 1): a chunked body without length would stall
    //until the transfer timeout. The sum is counted below instead.
    NetworkLimits::limitDownload(reply, bound);

    //The reader drains the reply at every readyRead, so the reply's own check
    //never sees the sum: the total is counted here.
    auto accumulate = [this]()
    {
        //The bound's abort() runs finished synchronously: reply is already null.
        if (!reply || reply->error() == QNetworkReply::OperationCanceledError)
            return;
        auto d = reply->readAll();
        if (dataRead.size() + d.size() > bound)
        {
            failure = QStringLiteral("Response larger than the allowed size");
            reply->abort();
            return;
        }
        emit dataReadyRead(d);
        dataRead.append(d);
    };

    if (resType == TypeJson)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinishedJson);
        connect(reply, &QNetworkReply::readyRead, this, accumulate);
    }
    else if (resType == TypeRawData)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinishedData);
        connect(reply, &QNetworkReply::readyRead, this, accumulate);
    }
    else if (resType == TypeFile)
    {
        connect(reply, &QNetworkReply::finished,
                this, &NetworkRequest::nmFinished);
        connect(reply, &QNetworkReply::readyRead,
                this, &NetworkRequest::nmReadyRead);
    }

    return true;
}

qint64 NetworkRequest::bodyLimit() const
{
    if (maxBytes > 0)
        return maxBytes;
    if (resType == TypeJson)
        return NetworkLimits::MaxJsonBytes;
    if (resType == TypeRawData)
        return NetworkLimits::MaxPictureBytes;
    return NetworkLimits::MaxFileBytes;
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
    if (!writeChunk(d))
        return;
    emit dataReadyRead(d);
}

bool NetworkRequest::storeChunk(const QByteArray &d)
{
    if (!dlFile)
        failure = QStringLiteral("The download file no longer exists");
    else if (written + d.size() > bound)
        failure = QStringLiteral("Download larger than the allowed size");
    //QFile buffers: a full disk only shows at flush.
    else if (dlFile->write(d) != d.size() || !dlFile->flush())
        failure = QStringLiteral("Cannot write the download file: ") + dlFile->errorString();
    else
    {
        written += d.size();
        return true;
    }
    return false;
}

bool NetworkRequest::writeChunk(const QByteArray &d)
{
    if (storeChunk(d))
        return true;
    reply->abort();
    return false;
}

//A retry with the same QFile must not append to what a failed attempt left.
void NetworkRequest::discardPartialFile()
{
    if (!dlFile)
        return;
    //A full disk makes resize(0) fail too: the partial body would stay on disk.
    const bool emptied = dlFile->resize(0);
    const QString name = dlFile->fileName();
    dlFile->close();
    if (!emptied && ownsOpen && QFileInfo(name).isFile() && !QFile::remove(name))
        qCWarning(lcNet) << "Cannot remove the partial download file";
}

void NetworkRequest::nmFinished()
{
    httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    networkError = reply->error();

    if (reply->error() != QNetworkReply::NoError)
    {
        //Our own abort (write error, size) reads as a bare "canceled".
        lastError = failure.isEmpty() ? LogRedact::redactError(reply->errorString(), reply->url()) : failure;
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        discardPartialFile();
        //State is reset before the signal: a slot may restart or destroy the request.
        reply->deleteLater();
        reply = nullptr;
        emit finished(RequestHttpError);

        return;
    }

    int status = RequestSuccess;
    //Finished: abort() would be ignored, the error is reported here.
    bool stored = true;
    if (!dlFile)
    {
        failure = QStringLiteral("The download file no longer exists");
        stored = false;
    }
    else if (reply->bytesAvailable() > 0)
        stored = storeChunk(reply->readAll());
    if (!stored)
    {
        lastError = failure;
        status = RequestHttpError;
        discardPartialFile();
    }
    else
        dlFile->close();

    reply->deleteLater();
    reply = nullptr;

    emit finished(status);
}

void NetworkRequest::nmFinishedJson()
{
    QJsonParseError err;
    QJsonDocument jdoc;

    httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    networkError = reply->error();

    if (reply->error() != QNetworkReply::NoError)
    {
        lastError = failure.isEmpty() ? LogRedact::redactError(reply->errorString(), reply->url()) : failure;
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        jdoc = QJsonDocument::fromJson(dataRead, &err);
        reply->deleteLater();
        reply = nullptr;
        emit finishedJson(RequestHttpError, jdoc);

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
        lastError = failure.isEmpty() ? LogRedact::redactError(reply->errorString(), reply->url()) : failure;
        qCWarning(lcNet) << "Error in" << LogRedact::redactUrl(reply->url()) << ":" << lastError
                         << "(status" << httpStatusCode << ")";
        reply->deleteLater();
        reply = nullptr;
        emit finishedData(RequestHttpError, QByteArray());

        return;
    }

    reply->deleteLater();
    reply = nullptr;
    emit finishedData(RequestSuccess, dataRead);
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
