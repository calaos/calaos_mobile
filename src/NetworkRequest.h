#ifndef NETWORKREQUEST_H
#define NETWORKREQUEST_H

#include <QtCore>
#include <QtNetwork>

#include "NetworkLimits.h"

class NetworkRequest: public QObject
{
    Q_OBJECT

public:
    enum HttpType
    {
        HttpGet,
        HttpPost,
        HttpPut,
        HttpDelete,
        HttpHead,
    };

    enum RequestStatus
    {
        RequestSuccess,
        RequestHttpError,
        RequestJsonParseError
    };

    enum ResulType
    {
        TypeJson, //default
        TypeRawData,
        TypeFile
    };

public:
    NetworkRequest(QObject *parent = nullptr);
    NetworkRequest(QString url, HttpType type = HttpGet, QObject *parent = nullptr);
    ~NetworkRequest();

    void setNetManager(QNetworkAccessManager *netManager);

    void setUrl(QString u) { url = u; }
    const QString &getUrl() const { return url; }

    void setRequestType(HttpType t) { type = t; }
    HttpType getRequestType() { return type; }

    void setResultType(ResulType t) { resType = t; }
    ResulType getResultType() { return resType; }

    void setPostData(const QByteArray &data) { postData = data; }
    const QByteArray &getPostData() const { return postData; }

    //Not owned, and may be destroyed before the request: the transfer then fails.
    //On a failure the file is closed and emptied. If it cannot be emptied (disk
    //full) and start() opened it, the path is removed (a symlink itself, not its
    //target); a failed removal is only logged. A file the caller opened is never
    //removed, nor one destroyed during the transfer.
    void setFile(QFile *f) { dlFile = f; }
    QFile *getFile() const { return dlFile; }
    //Bound on the body, counted as the reader consumes it. By default the one
    //of the result type (NetworkLimits).
    void setMaxBytes(qint64 max) { maxBytes = max; }

    void setCustomHeader(QString header, QString value);

    QString getLastError() { return lastError; }

    int getHttpStatusCode() const { return httpStatusCode; }
    QNetworkReply::NetworkError getNetworkError() const { return networkError; }

    void setCertificate(QString path);

public slots:
    bool start();
    void cancel();

signals:
    void finishedJson(int status, const QJsonDocument &doc);
    void finishedData(int status, const QByteArray &data);
    void finished(int status);
    void dataReadyRead(const QByteArray &data);

private slots:
    void nmFinishedJson();
    void nmFinishedData();
    void nmFinished();
    void nmReadyRead();
    void nmSslErrors(const QList<QSslError> &errors);

private:
    QNetworkAccessManager *netmanager = nullptr;
    QNetworkReply *reply = nullptr;

    QByteArray postData;
    QString url;
    HttpType type = HttpGet;
    ResulType resType = TypeJson;

    QMap<QString, QString> headers;

    QPointer<QFile> dlFile;
    bool ownsOpen = false;
    qint64 maxBytes = 0;
    //Fixed at start(), for every result type.
    qint64 bound = 0;
    qint64 written = 0;
    QString failure;
    qint64 bodyLimit() const;
    bool writeChunk(const QByteArray &d);
    bool storeChunk(const QByteArray &d);
    void discardPartialFile();
    QByteArray dataRead;

    bool delNm = false;

    QString lastError;
    int httpStatusCode = 0;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;

    QList<QSslCertificate> cacerts;
    QSslConfiguration sslConfiguration;
};

#endif // NETWORKREQUEST_H
