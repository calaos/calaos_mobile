#include "CalaosConnection.h"
#include <QJsonDocument>
#include <QDebug>

CalaosConnection::CalaosConnection(QObject *parent) :
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    connect(accessManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));
}

void CalaosConnection::sslErrors(QNetworkReply *reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}

void CalaosConnection::login(QString user, QString pass, QString h)
{
    username = user;
    password = pass;
    host = h;

    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = QStringLiteral("get_home");
    QJsonDocument jdoc(jroot);

    connect(accessManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(loginFinished(QNetworkReply*)));

    QUrl url(QString("https://%1/api.php").arg(host));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    accessManager->post(request, jdoc.toJson());
}

void CalaosConnection::loginFinished(QNetworkReply *reply)
{
    disconnect(accessManager, SIGNAL(finished(QNetworkReply*)),
               this, SLOT(loginFinished(QNetworkReply*)));

    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << reply->url() << ":" << reply->errorString();
        emit loginFailed();
        return;
    }

    QByteArray bytes = reply->readAll();
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

    if (err.error != QJsonParseError::NoError)
    {
        qDebug() << "JSON parse error " << err.errorString();
        emit loginFailed();
        return;
    }
    QVariantMap jroot = jdoc.object().toVariantMap();
    emit homeLoaded(jroot);

    connect(accessManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(requestFinished(QNetworkReply*)));
}

void CalaosConnection::requestFinished(QNetworkReply *reply)
{
    //TODO?
}

void CalaosConnection::sendCommand(QString id, QString value, QString type, QString action)
{
    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = action;
    jroot["type"] = type;
    jroot["id"] = id;
    jroot["value"] = value;
    QJsonDocument jdoc(jroot);

    QUrl url(QString("https://%1/api.php").arg(host));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    accessManager->post(request, jdoc.toJson());
}
