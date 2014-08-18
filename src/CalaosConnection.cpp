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
    uuidPolling.clear();

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

    qDebug() << jdoc.toJson();

    if (err.error != QJsonParseError::NoError)
    {
        qDebug() << "JSON parse error " << err.errorString();
        emit loginFailed();
        return;
    }
    QVariantMap jroot = jdoc.object().toVariantMap();

    //start polling
    startJsonPolling();

    emit homeLoaded(jroot);
}

void CalaosConnection::requestFinished()
{
    if (reqReply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << reqReply->url() << ":" << reqReply->errorString();
        return;
    }

    QByteArray bytes = reqReply->readAll();
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

    if (err.error != QJsonParseError::NoError)
    {
        qDebug() << "JSON parse error " << err.errorString();
        return;
    }

    qDebug() << jdoc.toJson();

    if (cbObject)
    {
        QVariantMap jroot = jdoc.object().toVariantMap();
        QMetaObject::invokeMethod(cbObject,
                                  cbMember.toLatin1().constData(),
                                  Qt::DirectConnection,
                                  Q_ARG(QVariantMap, jroot));
        cbObject = nullptr;
        cbMember.clear();
    }
}

void CalaosConnection::requestError(QNetworkReply::NetworkError code)
{
    qDebug() << "Request error!";
    emit disconnected();
    return;
}

void CalaosConnection::sendCommand(QString id, QString value, QString type, QString action)
{
    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = action;
    jroot["type"] = type;
    if (type == "audio")
        jroot["player_id"] = id;
    else
        jroot["id"] = id;
    jroot["value"] = value;
    QJsonDocument jdoc(jroot);

    QUrl url(QString("https://%1/api.php").arg(host));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    reqReply = accessManager->post(request, jdoc.toJson());

    connect(reqReply, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(reqReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));
}

void CalaosConnection::queryState(QStringList inputs, QStringList outputs, QStringList audio_players, QObject *obj, QString member)
{
    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = "get_state";
    jroot["inputs"] = QJsonValue::fromVariant(inputs);
    jroot["outputs"] = QJsonValue::fromVariant(outputs);
    jroot["audio_players"] = QJsonValue::fromVariant(audio_players);
    QJsonDocument jdoc(jroot);

    QUrl url(QString("https://%1/api.php").arg(host));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    reqReply = accessManager->post(request, jdoc.toJson());

    cbObject = obj;
    cbMember = member;

    connect(reqReply, SIGNAL(finished()), this, SLOT(requestFinished()));
    connect(reqReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));
}

void CalaosConnection::startJsonPolling()
{
    if (uuidPolling.isEmpty())
        qDebug() << "Start polling...";

    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = "poll_listen";
    if (uuidPolling.isEmpty())
        jroot["type"] = "register";
    else
    {
        jroot["type"] = "get";
        jroot["uuid"] = uuidPolling;
    }
    QJsonDocument jdoc(jroot);

    QUrl url(QString("https://%1/api.php").arg(host));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    pollReply = accessManager->post(request, jdoc.toJson());

    connect(pollReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));
    connect(pollReply, &QNetworkReply::finished, [=]()
    {
        if (pollReply->error() != QNetworkReply::NoError)
        {
            qDebug() << "Error in " << pollReply->url() << ":" << pollReply->errorString();
            emit disconnected();
            return;
        }

        QByteArray bytes = pollReply->readAll();
        QJsonParseError err;
        QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

        if (err.error != QJsonParseError::NoError)
        {
            qDebug() << "JSON parse error " << err.errorString();
            emit disconnected();
            return;
        }
        QVariantMap jroot = jdoc.object().toVariantMap();

        if (jroot.contains("uuid") && uuidPolling.isEmpty())
        {
            uuidPolling = jroot["uuid"].toString();
            startJsonPolling();
            return;
        }

        if (jroot["success"].toString() != "true")
        {
            qDebug() << "Failed to get events";
            emit disconnected();
            return;
        }

        QVariantList events = jroot["events"].toList();
        foreach (QVariant v, events)
            processEvents(v.toString());

        QTimer::singleShot(200, this, SLOT(startJsonPolling()));
    });
}

void CalaosConnection::processEvents(QString msg)
{
    if (msg == "") return;

    qDebug() << "Received: " << msg;

    QStringList spl = msg.split(' ');

    if (spl.at(0) == "output" || spl.at(0) == "input")
    {
        if (spl.size() < 3) return;

        QString id = QUrl::fromPercentEncoding(spl.at(1).toLocal8Bit());
        QStringList s = QUrl::fromPercentEncoding(spl.at(2).toLocal8Bit()).split(':');
        QString val;
        if (s.size() > 1) val = s.at(1);

        if (spl.at(0) == "input")
            emit eventInputChange(id, s.at(0), val);
        else
            emit eventOutputChange(id, s.at(0), val);
    }
    else if (spl.at(0) == "audio_volume")
    {
        if (spl.count() < 4) return;

        emit eventAudioVolumeChange(spl.at(1), spl.at(3).toDouble());
    }
    else if (spl.at(0) == "audio_status")
    {
        emit eventAudioStatusChange(spl.at(1), spl.at(2));
    }
    else if (spl.at(0) == "audio")
    {
        if (spl.count() > 2 &&
            spl.at(2) == "song")
            emit eventAudioChange(spl.at(1));
    }

    //TODO all other event types
}
