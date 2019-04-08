#include "CalaosConnection.h"
#include <QJsonDocument>
#include <QDebug>
#include "HardwareUtils.h"

CalaosConnection::CalaosConnection(QObject *parent) :
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    accessManagerCam = new QNetworkAccessManager(this);
    pollReply = nullptr;
    connect(accessManager, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));
    connect(accessManagerCam, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError> &)),
            this, SLOT(sslErrors(QNetworkReply*, const QList<QSslError> &)));
}

void CalaosConnection::sslErrors(QNetworkReply *reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}

void CalaosConnection::sslErrorsWebsocket(const QList<QSslError> &)
{
    wsocket->ignoreSslErrors();
}

void CalaosConnection::login(QString user, QString pass, QString h)
{
    if (constate != ConStateUnknown)
        return;

    constate = ConStateUnknown;

    HardwareUtils::Instance()->showNetworkActivity(true);

    username = user;
    password = pass;
    uuidPolling.clear();

    if (h.startsWith("http://") || h.startsWith("https://"))
    {
        httphost = h;
        wshost = h.replace("http", "ws");
        connectHttp(httphost);
    }
    else if (h.startsWith("ws://") || h.startsWith("wss://"))
    {
        wshost = h;
        httphost = h.replace("ws", "http");
        connectWebsocket(wshost);
    }
    else
    {
        //First try with websocket
#ifdef CALAOS_DESKTOP
        wshost = QString("ws://%1:5454/api").arg(h);
        httphost = QString("http://%1:5454/api.php").arg(h);
#else
        wshost = QString("wss://%1/api").arg(h);
        httphost = QString("https://%1/api.php").arg(h);
#endif
        constate = ConStateTryWebsocket;
        connectWebsocket(wshost);
    }
}

void CalaosConnection::connectHttp(QString h)
{
    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = QStringLiteral("get_home");
    QJsonDocument jdoc(jroot);

    connect(accessManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(loginFinished(QNetworkReply*)));

    QUrl url(h);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    accessManager->post(request, jdoc.toJson());
}

void CalaosConnection::connectWebsocket(QString h)
{
    qDebug() << "Trying to connect with websocket to: " << h;

    if (!wsocket)
    {
        wsocket = new QWebSocket();
        connect(wsocket, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsWebsocket(QList<QSslError>)));
        connect(wsocket, &QWebSocket::connected, this, &CalaosConnection::onWsConnected);
        connect(wsocket, &QWebSocket::disconnected, this, &CalaosConnection::onWsDisconnected);
        connect(wsocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onWsError()));
    }

    wsocket->open(h);
}

void CalaosConnection::closeWebsocket()
{
    if (wsocket)
    {
        wsocket->deleteLater();
        wsocket = nullptr;
    }

    if (wsPingTimeout)
    {
        delete wsPingTimeout;
        wsPingTimeout = nullptr;
    }

    if (wsPing)
    {
        delete wsPing;
        wsPing = nullptr;
    }

    if (constate != ConStateTryWebsocket)
        constate = ConStateUnknown;
}

void CalaosConnection::onWsConnected()
{
    qDebug() << "Websocket connected";
    connect(wsocket, &QWebSocket::textMessageReceived, this, &CalaosConnection::onWsTextMessageReceived);

    QJsonObject jroot, jdata;
    jroot["msg"] = QStringLiteral("login");
    jdata["cn_user"] = username;
    jdata["cn_pass"] = password;
    jroot["data"] = jdata;
    QJsonDocument jdoc(jroot);

    //Do login procedure
    wsocket->sendTextMessage(jdoc.toJson());

    connect(wsocket, &QWebSocket::pong, this, [=](quint64 elapsedTime, const QByteArray &payload)
    {
        if (elapsedTime > 1000)
            qWarning() << "Websocket PING/PONG took " << elapsedTime << "ms. Is your network slow?";
        if (payload != "calaos_ping")
            qWarning() << "Websocket PONG received wrong payload: " << payload;

        //reset timeout timer
        wsPingTimeout->start();
    });

    wsPing = new QTimer(this);
    connect(wsPing, &QTimer::timeout, this, [=]()
    {
        wsocket->ping("calaos_ping");
    });
    wsPing->start(5 * 1000); //every 5s send a ping to calaos_server

    wsPingTimeout = new QTimer(this);
    connect(wsPingTimeout, &QTimer::timeout, this, [=]()
    {
        qWarning() << "Websocket connection timeout, disconnect!";
        logout();
    });
    wsPingTimeout->start(20 * 1000); //20s timeout
    
    QTimer::singleShot(100, this, [=]()
    {
        if (!wsocket) return;
        if (HardwareUtils::Instance()->getDeviceToken().isEmpty()) return;
        
#if defined(Q_OS_ANDROID)
        QString hw = QStringLiteral("android");
#elif defined(Q_OS_IOS)
        QString hw = QStringLiteral("ios");
#else
        QString hw;
#endif

        //Send the push device token to register the mobile device to calaos_server for push notif
        sendWebsocket(QStringLiteral("register_push"),
                      {{ "token", HardwareUtils::Instance()->getDeviceToken() },
                       { "hardware", hw }});
    });
}

void CalaosConnection::onWsDisconnected()
{
    qDebug() << "Websocket disconnected";

    closeWebsocket();

    if (constate == ConStateTryWebsocket)
        connectHttp(httphost);
    else
        emit loginFailed();
}

void CalaosConnection::onWsError()
{
    if (!wsocket) return;

    qDebug() << "Websocket error: " << wsocket->errorString();

    closeWebsocket();

    if (constate == ConStateTryWebsocket)
        connectHttp(httphost);
    else
        emit loginFailed();
}

void CalaosConnection::logout()
{
    HardwareUtils::Instance()->showNetworkActivity(false);

    constate = ConStateUnknown;

    closeWebsocket();

    if (pollReply)
    {
        pollReply->abort();
        pollReply->deleteLater();
        pollReply = nullptr;
    }

    foreach (QNetworkReply *reply, reqReplies)
    {
        reply->abort();
        reply->deleteLater();
    }
    reqReplies.clear();

    uuidPolling.clear();
    emit disconnected();
}

void CalaosConnection::loginFinished(QNetworkReply *reply)
{
    HardwareUtils::Instance()->showNetworkActivity(false);

    disconnect(accessManager, SIGNAL(finished(QNetworkReply*)),
               this, SLOT(loginFinished(QNetworkReply*)));

    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << reply->url() << ":" << reply->errorString();
        constate = ConStateUnknown;
        emit loginFailed();
        return;
    }

    QByteArray bytes = reply->readAll();
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

#ifdef QT_DEBUG
    qDebug().noquote() << "RECV: " << jdoc.toJson();
#endif

    if (err.error != QJsonParseError::NoError)
    {
        qDebug() << "JSON parse error " << err.errorString();
        constate = ConStateUnknown;
        emit loginFailed();
        return;
    }

    //Connection success
    constate = ConStateHttp;
    QVariantMap jroot = jdoc.object().toVariantMap();
    
    if (!HardwareUtils::Instance()->getDeviceToken().isEmpty())
    {
#if defined(Q_OS_ANDROID)
        QString hw = QStringLiteral("android");
#elif defined(Q_OS_IOS)
        QString hw = QStringLiteral("ios");
#else
        QString hw;
#endif

        //Send the push device token to register the mobile device to calaos_server for push notif
        QJsonObject o = {
            { "token", HardwareUtils::Instance()->getDeviceToken() },
            { "hardware", hw }
        };
        sendHttp(QStringLiteral("register_push"), o);
    }

    //start polling
    startJsonPolling();

    emit homeLoaded(jroot);
}

void CalaosConnection::requestFinished()
{
    HardwareUtils::Instance()->showNetworkActivity(false);

    QNetworkReply *reqReply = qobject_cast<QNetworkReply*>(QObject::sender());

    if (!reqReply)
    {
        qWarning() << "Error reqReply is NULL!";
        return;
    }

    reqReply->deleteLater();

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
        qDebug() << bytes;
        qDebug() << "JSON parse error at " << err.offset << " : " << err.errorString();
        return;
    }

#ifdef QT_DEBUG
    qDebug().noquote() << "RECV: " << jdoc.toJson();
#endif

    reqReplies.removeAll(reqReply);

    QVariantMap jroot = jdoc.object().toVariantMap();

    if (jroot.contains("audio_players") &&
        !jroot["audio_players"].toList().isEmpty())
    {
        //emit event for audio player change
        emit eventAudioStateChange({}, jroot);
    }

    if (jroot.contains("events") &&
        jroot.contains("total_page"))
    {
        emit logEventLoaded(jroot["data"].toMap());
    }

    if (isHttpApiV2())
    {
        if (jroot.contains("inputs") &&
            !jroot["inputs"].toList().isEmpty())
        {
            //emit event for specific input change
            emit eventInputStateChange(jroot);
        }

        if (jroot.contains("outputs") &&
            !jroot["outputs"].toList().isEmpty())
        {
            //emit event for output change
            emit eventOutputStateChange(jroot);
        }
    }
    else
    {
        for (auto it = jroot.constBegin();it != jroot.constEnd();it++)
        {
            if (it.value().canConvert<QString>())
            {
                QVariantMap m = { { "id", it.key() },
                                  { "state", it.value().toString() }};
                emit eventInputStateChange(m);
                emit eventOutputStateChange(m);
            }
            else
            {
                emit eventAudioStateChange(it.key(), it.value().toMap());
            }
        }
    }
}

void CalaosConnection::requestCamFinished(QNetworkReply *reqReply, const QString &camid)
{
    if (!reqReply)
    {
        qWarning() << "Error reqReply is NULL!";
        return;
    }

    reqReply->deleteLater();

    if (reqReply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << reqReply->url() << ":" << reqReply->errorString();
        emit cameraPictureFailed(camid);
        return;
    }

    QByteArray bytes = reqReply->readAll();
    reqReplies.removeAll(reqReply);

    //we have a new picture
    emit cameraPictureDownloaded(camid, bytes);
}

void CalaosConnection::requestError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code)
    qDebug() << "Request error!" << code;
    logout();
    return;
}

void CalaosConnection::sendWebsocket(const QString &msg, const QJsonObject &data, const QString &client_id)
{
    if (!isWebsocket()) return;

    QJsonObject o = {{ "msg", msg },
                     { "data", data }};
    if (!client_id.isEmpty())
        o["msg_id"] = client_id;

    QJsonDocument doc(o);
#ifdef QT_DEBUG
    qDebug().noquote() << "SEND: " << doc.toJson();
#endif

    wsocket->sendTextMessage(doc.toJson());
}

void CalaosConnection::sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors)
{
    if (!isHttp()) return;

    if (!msg.isEmpty())
        data["action"] = msg;

    QJsonDocument doc(data);
#ifdef QT_DEBUG
    qDebug().noquote() << "SEND: " << doc.toJson();
#endif

    QUrl url(httphost);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reqReply = accessManager->post(request, doc.toJson());

    connect(reqReply, SIGNAL(finished()), this, SLOT(requestFinished()));
    if (!ignoreErrors)
        connect(reqReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));

    reqReplies.append(reqReply);
}

void CalaosConnection::sendCommand(QString id, QString value, QString type, QString action)
{
    HardwareUtils::Instance()->showNetworkActivity(true);

    QJsonObject jroot;
    if (isHttp())
    {
        jroot["cn_user"] = username;
        jroot["cn_pass"] = password;
        jroot["type"] = type;
    }
    if (type == "audio" && isHttpApiV2())
        jroot["player_id"] = id;
    else
        jroot["id"] = id;
    jroot["value"] = value;

    if (isWebsocket())
        sendWebsocket(action, jroot, "user_cmd");
    else
        sendHttp(action, jroot);
}

void CalaosConnection::sendJson(QString action, QJsonObject &jsonData)
{
    if (isWebsocket())
        sendWebsocket(action, jsonData, "user_cmd_json");
    else
    {
        jsonData["cn_user"] = username;
        jsonData["cn_pass"] = password;

        sendHttp(action, jsonData);
    }
}

void CalaosConnection::queryState(QStringList inputs, QStringList outputs, QStringList audio_players)
{
    HardwareUtils::Instance()->showNetworkActivity(true);

    QJsonObject jroot;
    if (isHttp())
    {
        jroot["cn_user"] = username;
        jroot["cn_pass"] = password;
    }

    if (isHttpApiV2())
    {
        jroot["inputs"] = QJsonValue::fromVariant(inputs);
        jroot["outputs"] = QJsonValue::fromVariant(outputs);
        jroot["audio_players"] = QJsonValue::fromVariant(audio_players);
    }
    else
    {
        QStringList io = inputs;
        io.append(outputs);
        io.append(audio_players);
        jroot["items"] = QJsonValue::fromVariant(io);
    }

    if (isWebsocket())
        sendWebsocket("get_state", jroot, "user_cmd");
    else
        sendHttp("get_state", jroot);
}

void CalaosConnection::getCameraPicture(const QString &camid, QString urlSuffix)
{
    QString u = httphost;
    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = QString("camera");
    if (!urlSuffix.isEmpty()) //v1 camera api
    {
        jroot["action"] = QStringLiteral("get_camera_pic");
        u = QStringLiteral("%1%2&u=%3&p=%4")
            .arg(httphost.left(httphost.length() - 8)) // "/api.php" is 9
            .arg(urlSuffix)
            .arg(QString(QUrl::toPercentEncoding(username)))
            .arg(QString(QUrl::toPercentEncoding(password)));
    }
    jroot["type"] = QString("get_picture");
    jroot["id"] = camid;
    jroot["camera_id"] = camid;
    QJsonDocument jdoc(jroot);

#ifdef QT_DEBUG
    qDebug().noquote() << "SEND: " << jdoc.toJson();
#endif

    QUrl url(u);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reqReply = accessManagerCam->post(request, jdoc.toJson());

    connect(reqReply, &QNetworkReply::finished, this, [=]() { requestCamFinished(reqReply, camid); });
//    connect(reqReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));

    reqReplies.append(reqReply);
}

void CalaosConnection::startJsonPolling()
{
    if (constate != ConStateHttp)
        return;

    if (uuidPolling.isEmpty())
        qDebug() << "Start polling...";

    QJsonObject jroot;
    jroot["cn_user"] = username;
    jroot["cn_pass"] = password;
    jroot["action"] = QString("poll_listen");
    if (uuidPolling.isEmpty())
        jroot["type"] = QString("register");
    else
    {
        jroot["type"] = QString("get");
        jroot["uuid"] = uuidPolling;
    }
    QJsonDocument jdoc(jroot);

    QUrl url(httphost);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    pollReply = accessManager->post(request, jdoc.toJson());

    connect(pollReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(requestError(QNetworkReply::NetworkError)));
    connect(pollReply, &QNetworkReply::finished, this, [=]()
    {
        pollReply->deleteLater();
        if (pollReply->error() != QNetworkReply::NoError)
        {
            qDebug() << "Error in " << pollReply->url() << ":" << pollReply->errorString();
            logout();
            return;
        }

        QByteArray bytes = pollReply->readAll();
        pollReply = nullptr;

        QJsonParseError err;
        QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

        //qDebug().noquote() << "RECV: " << jdoc.toJson();

        if (err.error != QJsonParseError::NoError)
        {
            qDebug() << "JSON parse error " << err.errorString();
            qDebug() << "Failing data: " << bytes;
            logout();
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
            logout();
            emit disconnected();
            return;
        }

        QVariantList events = jroot["events"].toList();
        foreach (QVariant v, events)
        {
            if (isHttpApiV2())
                processEventsV2(v.toString());
            else
                processEventsV3(v.toMap());
        }

        QTimer::singleShot(200, this, SLOT(startJsonPolling()));
    });
}

void CalaosConnection::processEventsV2(QString msg)
{
    if (msg == "") return;

#ifdef QT_DEBUG
    qDebug().noquote() << "Received: " << msg;
#endif

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
            spl.at(2) == "songchanged")
            emit eventAudioChange(spl.at(1));
    }

    //TODO all other event types
}

void CalaosConnection::processEventsV3(QVariantMap msg)
{
#ifdef QT_DEBUG
    qDebug().noquote() << "Received: " << msg["event_raw"];
#endif

    QVariantMap data = msg["data"].toMap();
    if (msg["type_str"].toString() == "io_changed")
    {
        for (auto it = data.begin();it != data.end();it++)
        {
            if (it.key() == "id") continue;
            emit eventInputChange(data["id"].toString(), it.key(), it.value().toString());
            emit eventOutputChange(data["id"].toString(), it.key(), it.value().toString());
        }
    }
    else if (msg["type_str"].toString() == "audio_volume_changed")
    {
        emit eventAudioVolumeChange(data["player_id"].toString(), data["volume"].toString().toDouble());
    }
    else if (msg["type_str"].toString() == "audio_status_changed")
    {
        emit eventAudioStatusChange(data["player_id"].toString(), data["state"].toString());
    }
    else if (msg["type_str"].toString() == "audio_song_changed")
    {
        emit eventAudioChange(data["player_id"].toString());
    }
    else if (msg["type_str"].toString() == "touchscreen_camera_request")
    {
        emit eventTouchscreenCamera(data["id"].toString());
    }

    //TODO all other event types
}

void CalaosConnection::onWsTextMessageReceived(const QString &message)
{
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(message.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError)
    {
        qWarning() << "JSON parse error " << err.errorString();

        if (constate == ConStateUnknown)
            emit loginFailed();

        return;
    }

    QJsonObject jroot = jdoc.object();
    QJsonObject jdata = jroot["data"].toObject();

#ifdef QT_DEBUG
    qDebug() << "RECV:" << message;
#endif

    if (jroot["msg"] == "login")
    {
        if (jdata["success"] == "true" && constate != ConStateWebsocket)
        {
            constate = ConStateWebsocket;

            //ask for home
            sendWebsocket("get_home");
        }
    }
    else if (jroot["msg"] == "get_home")
    {
        emit homeLoaded(jroot["data"].toObject().toVariantMap());
        HardwareUtils::Instance()->showNetworkActivity(false);
    }
    else if (jroot["msg"] == "event")
    {
        processEventsV3(jroot["data"].toObject().toVariantMap());
    }
    else if (jroot["msg"] == "get_state")
    {
        //emit event for specific input/output change
        for (auto it = jdata.constBegin();it != jdata.constEnd();it++)
        {
            if (it.value().isString())
            {
                QVariantMap m = { { "id", it.key() },
                                  { "state", it.value().toString() }};
                emit eventInputStateChange(m);
                emit eventOutputStateChange(m);
            }
            else
            {
                emit eventAudioStateChange(it.key(), it.value().toObject().toVariantMap());
            }
        }
    }
    else if (jroot["msg"] == "eventlog")
    {
        emit logEventLoaded(jroot["data"].toObject().toVariantMap());
    }

    //We get this marker when calling sendCommand(...) it helps disabling the net indicator
    if (jroot["msg_id"] == "user_cmd")
    {
        HardwareUtils::Instance()->showNetworkActivity(false);
    }
}

QString CalaosConnection::getNotifPictureUrl(const QString &pic_uid)
{
    return QStringLiteral("%1?cn_user=%2&cn_pass=%3&action=event_picture&pic_uid=%4")
            .arg(httphost)
            .arg(username)
            .arg(password)
            .arg(pic_uid);
}
