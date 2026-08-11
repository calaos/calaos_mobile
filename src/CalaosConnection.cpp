#include "CalaosConnection.h"
#include <QJsonDocument>
#include <QUrlQuery>
#include <QDebug>
#include "HardwareUtils.h"
#include "Common.h"
#include "JsonKeys.h"

//Query parameters and JSON keys that carry credentials and must never be logged
static const QStringList &credentialKeys()
{
    //"u" and "p" are not JSON keys but the short query parameters of the
    //legacy v1 camera url, so they stay literals here (see JsonKeys.h).
    static const QStringList keys = { JsonKeys::CnUser,
                                      JsonKeys::CnPass,
                                      QStringLiteral("u"),
                                      QStringLiteral("p") };
    return keys;
}

//Default transfer timeout for every "short" HTTP request (login, commands,
//state queries, camera pictures, audio covers). Those are all request/reply
//exchanges the server answers immediately, so 30s without a single byte
//moving means the link is dead, not slow.
static const int HTTP_TRANSFER_TIMEOUT = 30000;

//The long-poll (action "poll_listen") is the one exception: the server keeps
//the request deliberately open until an event happens, so a calm installation
//legitimately transfers nothing for a long time. It gets its own, much larger
//per request timeout, which QNetworkAccessManager does NOT override (the
//manager wide value is only used as a fallback when the request carries none).
//It still bounds a silently dead connection instead of hanging forever.
static const int HTTP_POLL_TRANSFER_TIMEOUT = 120000;

//Pause before a long-poll that failed is simply retried, while the session and
//the UI are kept as they are. Short on purpose: this is not the reconnection
//backoff, only a breather so a server answering instantly with an error cannot
//spin the event loop.
static const int POLL_RETRY_DELAY = 1000;

CalaosConnection::CalaosConnection(QObject *parent) :
    QObject(parent)
{
    accessManager = new QNetworkAccessManager(this);
    accessManagerCam = new QNetworkAccessManager(this);
    accessManager->setTransferTimeout(HTTP_TRANSFER_TIMEOUT);
    accessManagerCam->setTransferTimeout(HTTP_TRANSFER_TIMEOUT);
    pollReply = nullptr;
    connect(accessManager, &QNetworkAccessManager::sslErrors,
            this, &CalaosConnection::sslErrors);
    connect(accessManagerCam, &QNetworkAccessManager::sslErrors,
            this, &CalaosConnection::sslErrors);
}

void CalaosConnection::sslErrors(QNetworkReply *reply, const QList<QSslError> &)
{
    reply->ignoreSslErrors();
}

void CalaosConnection::sslErrorsWebsocket(const QList<QSslError> &)
{
    wsocket->ignoreSslErrors();
}

QString CalaosConnection::redactUrl(const QUrl &url)
{
    if (!url.hasQuery())
        return url.toString();

    const QList<QPair<QString, QString>> items = QUrlQuery(url).queryItems(QUrl::FullyEncoded);
    QUrlQuery redacted;
    bool changed = false;

    for (const auto &item: items)
    {
        if (credentialKeys().contains(item.first))
        {
            redacted.addQueryItem(item.first, QStringLiteral("***"));
            changed = true;
        }
        else
        {
            redacted.addQueryItem(item.first, item.second);
        }
    }

    if (!changed)
        return url.toString();

    QUrl u(url);
    u.setQuery(redacted);
    return u.toString();
}

QByteArray CalaosConnection::redactJson(const QJsonObject &obj)
{
    QJsonObject o = obj;
    for (const QString &key: credentialKeys())
    {
        if (o.contains(key))
            o[key] = QStringLiteral("***");
    }

    return QJsonDocument(o).toJson();
}

void CalaosConnection::emitStateChanged()
{
    if (lastEmittedState == reconnectPolicy.state())
        return;

    lastEmittedState = reconnectPolicy.state();
    emit connectionStateChanged(int(lastEmittedState));
}

void CalaosConnection::cancelReconnectTimer()
{
    if (reconnectTimer)
        reconnectTimer->stop();
}

void CalaosConnection::armReconnectTimer()
{
    if (!reconnectTimer)
    {
        reconnectTimer = new QTimer(this);
        reconnectTimer->setSingleShot(true);
        connect(reconnectTimer, &QTimer::timeout, this, &CalaosConnection::reconnectTimerFired);
    }

    //start() on an already running timer restarts it: there is physically only
    //one reconnection timer in the process, it can never be stacked.
    const int delay = reconnectPolicy.pendingDelayMs();
    qInfo() << "Reconnection attempt #" << reconnectPolicy.failureCount()
            << " scheduled in " << delay << "ms";
    reconnectTimer->start(delay);
}

void CalaosConnection::reconnectTimerFired()
{
    if (reconnectPolicy.state() != ReconnectPolicy::Reconnecting)
        return;

    startConnection(username, password, lasthost, ReconnectPolicy::AutoRetry);
}

void CalaosConnection::resetTransport()
{
    constate = ConStateUnknown;
    wsFallbackDone = false;
    demoMode = false;
    uuidPolling.clear();

    //The login handler is hooked on the manager, not on the reply: unhook it
    //before aborting anything, otherwise the abort of the *previous* attempt
    //would be reported as a failure of the *next* one.
    disconnect(accessManager, &QNetworkAccessManager::finished,
               this, &CalaosConnection::loginFinished);

    closeWebsocket();

    if (pollReply)
    {
        //Clearing the member first makes the finished() lambda of that reply
        //recognise itself as obsolete and bail out.
        QNetworkReply *reply = pollReply;
        pollReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }

    const QList<QNetworkReply *> replies = reqReplies;
    reqReplies.clear();
    foreach (QNetworkReply *reply, replies)
    {
        reply->abort();
        reply->deleteLater();
    }
}

void CalaosConnection::connectionLost(const QString &reason, ReconnectPolicy::Failure kind)
{
    if (reconnectPolicy.state() != ReconnectPolicy::Connecting &&
        reconnectPolicy.state() != ReconnectPolicy::Connected)
    {
        //Already torn down and already scheduled: a second failure report for
        //the same session must not restart anything.
        return;
    }

    qWarning() << "Calaos connection lost: " << reason;

    HardwareUtils::Instance()->showNetworkActivity(false);
    resetTransport();

    const bool retry = reconnectPolicy.failed(kind);

    //Notified before the state change so Application::loginFailed() still sees
    //the previous applicationStatus and can run its "already disconnected"
    //guard on a meaningful value.
    if (kind == ReconnectPolicy::AuthFailure && reconnectPolicy.hasGivenUp())
        emit loginFailed();
    else
        emit disconnected();

    emitStateChanged();

    if (retry)
        armReconnectTimer();
}

void CalaosConnection::requestImmediateReconnect(QString user, QString pass, QString h)
{
    //Discovery fires once per announce datagram, so it can be chatty. It is
    //allowed to short-circuit the delay currently pending, but not more than
    //once per second: otherwise a talkative LAN would defeat the backoff.
    if (lastAttemptTimer.isValid() &&
        lastAttemptTimer.elapsed() < ReconnectPolicy::BaseDelayMs)
        return;

    startConnection(user, pass, h, ReconnectPolicy::AutoRetry);
}

void CalaosConnection::suspend()
{
    HardwareUtils::Instance()->showNetworkActivity(false);
    cancelReconnectTimer();
    resetTransport();
    reconnectPolicy.suspend();
    emitStateChanged();
    emit disconnected();
}

void CalaosConnection::resume(QString user, QString pass, QString h)
{
    if (!reconnectPolicy.canResume())
        return;

    startConnection(user, pass, h, ReconnectPolicy::AutoRetry);
}

void CalaosConnection::login(QString user, QString pass, QString h)
{
    //An explicit login: the user (or the app coming back to the foreground)
    //asked for it, so the backoff and any previous give-up are reset.
    startConnection(user, pass, h, ReconnectPolicy::UserRequest);
}

void CalaosConnection::startConnection(QString user, QString pass, QString h, ReconnectPolicy::Trigger trigger)
{
    //Single anti double-login guard of the whole application.
    if (!reconnectPolicy.canBeginAttempt(trigger))
        return;

    //Tear the previous transport down first: aborting its replies can report
    //failures synchronously, and those must land while the policy still knows
    //about the old attempt.
    resetTransport();
    cancelReconnectTimer();
    reconnectPolicy.beginAttempt(trigger);
    lastAttemptTimer.restart();
    emitStateChanged();

    HardwareUtils::Instance()->showNetworkActivity(true);

    username = user;
    password = pass;
    lasthost = h;

    if ((user == "demo" || user == "demo@calaos.fr") &&
        pass == "demo" &&
        h == "demo.calaos.fr")
    {
        demoMode = true;

        //Wait some time for animation
        QTimer::singleShot(1000, this, [this]()
        {
            //A logout, or any newer attempt, disarms this one.
            if (!demoMode || reconnectPolicy.state() != ReconnectPolicy::Connecting)
                return;
            constate = ConStateWebsocket;
            reconnectPolicy.attemptSucceeded();
            emitStateChanged();
            HardwareUtils::Instance()->showNetworkActivity(false);
            emitHomeLoaded(QVariantMap());
        });

        return;
    }
    else
    {
        demoMode = false;
    }

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
    jroot[JsonKeys::CnUser] = username;
    jroot[JsonKeys::CnPass] = password;
    jroot[JsonKeys::Action] = QStringLiteral("get_home");
    QJsonDocument jdoc(jroot);

    //UniqueConnection: even if two paths ever managed to reach connectHttp()
    //for the same attempt, the answer must be handled exactly once.
    connect(accessManager, &QNetworkAccessManager::finished,
            this, &CalaosConnection::loginFinished, Qt::UniqueConnection);

    QUrl url(h);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    accessManager->post(request, jdoc.toJson());
}

void CalaosConnection::connectWebsocket(QString h)
{
    qDebug() << "Trying to connect with websocket to: " << redactUrl(QUrl(h));

    if (!wsocket)
    {
        wsocket = new QWebSocket();
        connect(wsocket, &QWebSocket::sslErrors,
                this, &CalaosConnection::sslErrorsWebsocket);
        connect(wsocket, &QWebSocket::connected, this, &CalaosConnection::onWsConnected);
        connect(wsocket, &QWebSocket::disconnected, this, &CalaosConnection::onWsDisconnected);
        //errorOccurred(), not the error() overload: error() is both a getter and
        //a signal on QWebSocket (so &QWebSocket::error is ambiguous) and the
        //signal is deprecated since Qt 6.5. Same signal, current name.
        connect(wsocket, &QWebSocket::errorOccurred, this, &CalaosConnection::onWsError);
    }

    wsocket->open(h);
}

void CalaosConnection::closeWebsocket()
{
    if (wsocket)
    {
        //Unhook first: deleteLater() does not close the socket right away, and
        //a late disconnected()/error() from a socket we already gave up on
        //used to re-enter the fallback and the failure handling.
        disconnect(wsocket, nullptr, this, nullptr);
        wsocket->deleteLater();
        wsocket = nullptr;
    }

    //deleteLater(), not delete: closeWebsocket() is now reachable from the
    //wsPingTimeout handler itself (a ping timeout is a lost connection), and
    //destroying a QTimer while its own timeout() is being emitted is a crash.
    if (wsPingTimeout)
    {
        wsPingTimeout->stop();
        wsPingTimeout->disconnect(this);
        wsPingTimeout->deleteLater();
        wsPingTimeout = nullptr;
    }

    if (wsPing)
    {
        wsPing->stop();
        wsPing->disconnect(this);
        wsPing->deleteLater();
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
    jroot[JsonKeys::Msg] = QStringLiteral("login");
    jdata[JsonKeys::CnUser] = username;
    jdata[JsonKeys::CnPass] = password;
    jroot[JsonKeys::Data] = jdata;
    QJsonDocument jdoc(jroot);

    //Do login procedure
    wsocket->sendTextMessage(jdoc.toJson());

    //Both timers must exist before the pong handler is connected, as the
    //handler restarts wsPingTimeout as soon as a pong is received.
    wsPing = new QTimer(this);
    connect(wsPing, &QTimer::timeout, this, [=]()
    {
        wsocket->ping("calaos_ping");
    });

    wsPingTimeout = new QTimer(this);
    connect(wsPingTimeout, &QTimer::timeout, this, [=]()
    {
        //Transient by nature: tear down and let the backoff bring us back.
        connectionLost(QStringLiteral("websocket ping timeout"));
    });

    connect(wsocket, &QWebSocket::pong, this, [=](quint64 elapsedTime, const QByteArray &payload)
    {
        if (elapsedTime > 1000)
            qWarning() << "Websocket PING/PONG took " << elapsedTime << "ms. Is your network slow?";
        if (payload != "calaos_ping")
            qWarning() << "Websocket PONG received wrong payload: " << payload;

        //reset timeout timer
        wsPingTimeout->start();
    });

    wsPing->start(5 * 1000); //every 5s send a ping to calaos_server
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
                      {{ JsonKeys::Token, HardwareUtils::Instance()->getDeviceToken() },
                       { JsonKeys::Hardware, hw }});
    });
}

void CalaosConnection::onWsDisconnected()
{
    qDebug() << "Websocket disconnected";

    handleWebsocketFailure(QStringLiteral("websocket disconnected"));
}

void CalaosConnection::onWsError()
{
    if (!wsocket) return;

    qDebug() << "Websocket error: " << wsocket->errorString();

    handleWebsocketFailure(wsocket->errorString());
}

void CalaosConnection::handleWebsocketFailure(const QString &reason)
{
    //A websocket that fails usually emits error() *and* disconnected(). Both
    //used to close the socket and start an HTTP login, giving two concurrent
    //logins. Only the first one through here may engage the fallback.
    const bool engageFallback = (constate == ConStateTryWebsocket) && !wsFallbackDone;

    closeWebsocket();

    if (engageFallback)
    {
        wsFallbackDone = true;
        qDebug() << "Websocket failed, falling back to HTTP";
        connectHttp(httphost);
        return;
    }

    if (constate == ConStateTryWebsocket)
    {
        //Fallback already engaged by the sibling signal, the HTTP login is in
        //flight: nothing to do, and above all nothing to start.
        return;
    }

    connectionLost(reason);
}

void CalaosConnection::logout()
{
    HardwareUtils::Instance()->showNetworkActivity(false);

    cancelReconnectTimer();
    resetTransport();
    //Explicit logout: no automatic reconnection is wanted any more.
    reconnectPolicy.stop();
    emitStateChanged();

    emit disconnected();
}

void CalaosConnection::loginFinished(QNetworkReply *reply)
{
    HardwareUtils::Instance()->showNetworkActivity(false);

    disconnect(accessManager, &QNetworkAccessManager::finished,
               this, &CalaosConnection::loginFinished);

    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reply->url()) << ":" << reply->errorString();

        //Credentials rejected by the server is not the same thing as a server
        //that is down: the first one must not be retried forever.
        const QVariant codeAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const int httpCode = codeAttr.isValid() ? Common::toIntSafe(codeAttr, 0, "login http status") : 0;
        //calaos_server answers a rejected login on /api.php with 400 and an
        //HTML body, not 401, so the success flag below is never reached: the
        //reply already carries a network error. Measured against a local
        //server: bad password -> 400, good password -> 200. Without 400 here a
        //wrong password over the HTTP transport retries forever and the login
        //screen never comes back.
        const bool isAuth = reply->error() == QNetworkReply::AuthenticationRequiredError ||
                            reply->error() == QNetworkReply::ContentAccessDenied ||
                            reply->error() == QNetworkReply::ProtocolInvalidOperationError ||
                            httpCode == 400 || httpCode == 401 || httpCode == 403;

        connectionLost(reply->errorString(),
                       isAuth ? ReconnectPolicy::AuthFailure : ReconnectPolicy::TransientFailure);
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
        connectionLost(QStringLiteral("login answer is not valid JSON"));
        return;
    }

    QVariantMap jroot = jdoc.object().toVariantMap();

    //The HTTP API reports a rejected login with an explicit success flag
    //rather than an HTTP error code.
    if (jroot.contains(JsonKeys::Success) &&
        jroot[JsonKeys::Success].toString() != "true")
    {
        connectionLost(QStringLiteral("server rejected the credentials"),
                       ReconnectPolicy::AuthFailure);
        return;
    }

    //Connection success
    constate = ConStateHttp;
    reconnectPolicy.attemptSucceeded();
    emitStateChanged();

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
            { JsonKeys::Token, HardwareUtils::Instance()->getDeviceToken() },
            { JsonKeys::Hardware, hw }
        };
        sendHttp(QStringLiteral("register_push"), o);
    }

    //start polling
    startJsonPolling();

    emitHomeLoaded(jroot);
}

void CalaosConnection::emitHomeLoaded(const QVariantMap &home)
{
    /* The API version is read here, off the payload this class just received,
     * and no longer written back into this class by RoomModel::load() while it
     * parses rooms (T18). Doing it before the signal also means every consumer
     * of homeLoaded() - not only the first model to parse a room - sees the
     * right value. */
    bool v2 = isV2HttpApi;
    if (detectHttpApiV2(home, v2))
        updateHttpApiV2(v2);

    emit homeLoaded(home);
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
        qDebug() << "Error in " << redactUrl(reqReply->url()) << ":" << reqReply->errorString();
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

    //Audio player states, event log page and io states can all be in the same
    //answer: the decoder reports every payload it recognises, in order.
    dispatchEvents(CalaosEventDecoder::decodeQueryAnswer(
                       jroot,
                       isHttpApiV2() ? CalaosEventDecoder::ApiV2
                                     : CalaosEventDecoder::ApiV3));
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
        qDebug() << "Error in " << redactUrl(reqReply->url()) << ":" << reqReply->errorString();
        emit cameraPictureFailed(camid);
        return;
    }

    QByteArray bytes = reqReply->readAll();
    reqReplies.removeAll(reqReply);

    //we have a new picture
    emit cameraPictureDownloaded(camid, bytes);
}

void CalaosConnection::requestAudioCoverFinished(QNetworkReply *reqReply, const QString &playerid)
{
    if (!reqReply)
    {
        qWarning() << "Error reqReply is NULL!";
        return;
    }

    reqReply->deleteLater();

    if (reqReply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reqReply->url()) << ":" << reqReply->errorString();
        return;
    }

    QByteArray bytes = reqReply->readAll();
    reqReplies.removeAll(reqReply);

    //we have a new picture
    emit audioCoverDownloaded(playerid, bytes);
}

void CalaosConnection::requestError(QNetworkReply::NetworkError code)
{
    //A single failed command or state query is not a disconnection: this used
    //to call logout(), which emptied every model of the UI. The link is only
    //declared dead by the websocket ping timeout or by a run of consecutive
    //long-poll failures (see handlePollFailure()).
    qWarning() << "Request error, keeping the session: " << code;
}

void CalaosConnection::sendWebsocket(const QString &msg, const QJsonObject &data, const QString &client_id)
{
    if (!isWebsocket()) return;

    QJsonObject o = {{ JsonKeys::Msg, msg },
                     { JsonKeys::Data, data }};
    if (!client_id.isEmpty())
        o[JsonKeys::MsgId] = client_id;

    QJsonDocument doc(o);
#ifdef QT_DEBUG
    //qDebug().noquote() << "SEND: " << doc.toJson();
#endif

    wsocket->sendTextMessage(doc.toJson());
}

void CalaosConnection::sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors)
{
    if (!isHttp()) return;

    if (!msg.isEmpty())
        data[JsonKeys::Action] = msg;

    QJsonDocument doc(data);
#ifdef QT_DEBUG
    qDebug().noquote() << "SEND: " << redactJson(data);
#endif

    QUrl url(httphost);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QNetworkReply *reqReply = accessManager->post(request, doc.toJson());

    connect(reqReply, &QNetworkReply::finished, this, &CalaosConnection::requestFinished);
    if (!ignoreErrors)
        //QNetworkReply has no error(NetworkError) signal since Qt 6 (only the
        //error() getter), so the old string based connect never resolved: this
        //connection was silently dead. errorOccurred() is the Qt 6 name.
        connect(reqReply, &QNetworkReply::errorOccurred, this, &CalaosConnection::requestError);

    reqReplies.append(reqReply);
}

void CalaosConnection::sendCommand(QString id, QString value, QString type, QString action)
{
    HardwareUtils::Instance()->showNetworkActivity(true);

    QJsonObject jroot;
    if (isHttp())
    {
        jroot[JsonKeys::CnUser] = username;
        jroot[JsonKeys::CnPass] = password;
        jroot[JsonKeys::Type] = type;
    }
    if (type == "audio" && isHttpApiV2())
        jroot[JsonKeys::PlayerId] = id;
    else
        jroot[JsonKeys::Id] = id;
    jroot[JsonKeys::Value] = value;

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
        jsonData[JsonKeys::CnUser] = username;
        jsonData[JsonKeys::CnPass] = password;

        sendHttp(action, jsonData);
    }
}

void CalaosConnection::queryState(QStringList inputs, QStringList outputs, QStringList audio_players)
{
    HardwareUtils::Instance()->showNetworkActivity(true);

    QJsonObject jroot;
    if (isHttp())
    {
        jroot[JsonKeys::CnUser] = username;
        jroot[JsonKeys::CnPass] = password;
    }

    if (isHttpApiV2())
    {
        jroot[JsonKeys::Inputs] = QJsonValue::fromVariant(inputs);
        jroot[JsonKeys::Outputs] = QJsonValue::fromVariant(outputs);
        jroot[JsonKeys::AudioPlayers] = QJsonValue::fromVariant(audio_players);
    }
    else
    {
        QStringList io = inputs;
        io.append(outputs);
        io.append(audio_players);
        jroot[JsonKeys::Items] = QJsonValue::fromVariant(io);
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
    jroot[JsonKeys::CnUser] = username;
    jroot[JsonKeys::CnPass] = password;
    jroot[JsonKeys::Action] = QString("camera");
    if (!urlSuffix.isEmpty()) //v1 camera api
    {
        jroot[JsonKeys::Action] = QStringLiteral("get_camera_pic");
        u = QStringLiteral("%1%2&u=%3&p=%4")
            .arg(httphost.left(httphost.length() - 8)) // "/api.php" is 9
            .arg(urlSuffix)
            .arg(QString(QUrl::toPercentEncoding(username)))
            .arg(QString(QUrl::toPercentEncoding(password)));
    }
    jroot[JsonKeys::Type] = QString("get_picture");
    jroot[JsonKeys::Id] = camid;
    jroot[JsonKeys::CameraId] = camid;
    QJsonDocument jdoc(jroot);

#ifdef QT_DEBUG
    //qDebug().noquote() << "SEND: " << jdoc.toJson();
#endif

    QUrl url(u);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reqReply = accessManagerCam->post(request, jdoc.toJson());

    connect(reqReply, &QNetworkReply::finished, this, [=]() { requestCamFinished(reqReply, camid); });
//    connect(reqReply, &QNetworkReply::errorOccurred, this, &CalaosConnection::requestError);

    reqReplies.append(reqReply);
}

void CalaosConnection::getAudioCover(const QString &playerid)
{
    QString u = httphost;
    QJsonObject jroot;
    jroot[JsonKeys::CnUser] = username;
    jroot[JsonKeys::CnPass] = password;
    jroot[JsonKeys::Action] = QString("audio");
    jroot[JsonKeys::AudioAction] = QString("get_cover");
    jroot[JsonKeys::Id] = playerid;
    QJsonDocument jdoc(jroot);

#ifdef QT_DEBUG
    //qDebug().noquote() << "SEND: " << jdoc.toJson();
#endif

    QUrl url(u);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reqReply = accessManagerCam->post(request, jdoc.toJson());

    connect(reqReply, &QNetworkReply::finished, this, [=]() { requestAudioCoverFinished(reqReply, playerid); });

    reqReplies.append(reqReply);
}

void CalaosConnection::changeCredentials(QString user, QString pass)
{
    //Only supported for websocket
    if (!isWebsocket()) return;

    username_temp = user;
    password_temp = pass;

    QJsonObject jroot = { { JsonKeys::Action, "change_cred" },
                          { JsonKeys::OldUser, username },
                          { JsonKeys::OldPw, password },
                          { JsonKeys::NewUser, user },
                          { JsonKeys::NewPw, pass },
                         };
    sendWebsocket("settings", jroot, "change_creds");
}

void CalaosConnection::handlePollFailure(const QString &reason)
{
    if (!reconnectPolicy.pollFailed())
    {
        //Enough consecutive failures: the link really is gone.
        connectionLost(QStringLiteral("long-poll failed %1 times in a row (%2)")
                       .arg(reconnectPolicy.pollFailureCount())
                       .arg(reason));
        return;
    }

    //Below the threshold the session and the eight UI models are left
    //untouched, we simply run the poll again.
    qWarning() << "Long-poll failure " << reconnectPolicy.pollFailureCount()
               << "/" << ReconnectPolicy::MaxPollFailures << ": " << reason
               << " - retrying without dropping the session";
    QTimer::singleShot(POLL_RETRY_DELAY, this, &CalaosConnection::startJsonPolling);
}

void CalaosConnection::startJsonPolling()
{
    if (constate != ConStateHttp)
        return;

    //Never two polls in flight: the retry timers above and the 200ms chaining
    //below could otherwise both land after a reconnection.
    if (pollReply)
        return;

    if (uuidPolling.isEmpty())
        qDebug() << "Start polling...";

    QJsonObject jroot;
    jroot[JsonKeys::CnUser] = username;
    jroot[JsonKeys::CnPass] = password;
    jroot[JsonKeys::Action] = QString("poll_listen");
    if (uuidPolling.isEmpty())
        jroot[JsonKeys::Type] = QString("register");
    else
    {
        jroot[JsonKeys::Type] = QString("get");
        jroot[JsonKeys::Uuid] = uuidPolling;
    }
    QJsonDocument jdoc(jroot);

    QUrl url(httphost);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    //Long-poll: the server holds this request open until an event shows up.
    //Setting the timeout on the request itself takes precedence over the 30s
    //default of accessManager, which would otherwise abort a perfectly healthy
    //poll on a quiet installation and trigger a logout/login loop.
    request.setTransferTimeout(HTTP_POLL_TRANSFER_TIMEOUT);
    QNetworkReply *reply = accessManager->post(request, jdoc.toJson());
    pollReply = reply;

    //The error is handled in finished() only. It used to be wired to
    //requestError() as well, which logged out on the very first hiccup and
    //emptied the whole UI.
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        if (pollReply != reply)
        {
            //A logout or a new connection already replaced this poll, its
            //answer (or its abort) is obsolete.
            reply->deleteLater();
            return;
        }

        pollReply = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError)
        {
            qDebug() << "Error in " << redactUrl(reply->url()) << ":" << reply->errorString();
            handlePollFailure(reply->errorString());
            return;
        }

        QByteArray bytes = reply->readAll();

        QJsonParseError err;
        QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);

        //qDebug().noquote() << "RECV: " << jdoc.toJson();

        if (err.error != QJsonParseError::NoError)
        {
            qDebug() << "JSON parse error " << err.errorString();
            qDebug() << "Failing data: " << bytes;
            handlePollFailure(QStringLiteral("invalid JSON in poll answer"));
            return;
        }
        QVariantMap jroot = jdoc.object().toVariantMap();

        if (jroot.contains(JsonKeys::Uuid) && uuidPolling.isEmpty())
        {
            reconnectPolicy.pollSucceeded();
            uuidPolling = jroot[JsonKeys::Uuid].toString();
            startJsonPolling();
            return;
        }

        if (jroot[JsonKeys::Success].toString() != "true")
        {
            qDebug() << "Failed to get events";
            handlePollFailure(QStringLiteral("poll answer reports a failure"));
            return;
        }

        reconnectPolicy.pollSucceeded();

        QVariantList events = jroot[JsonKeys::Events].toList();
        foreach (QVariant v, events)
        {
            if (isHttpApiV2())
                processEventsV2(v.toString());
            else
                processEventsV3(v.toMap());
        }

        QTimer::singleShot(200, this, &CalaosConnection::startJsonPolling);
    });
}

void CalaosConnection::dispatchEvents(const QList<DecodedEvent> &events)
{
    for (const DecodedEvent &ev: events)
    {
        //No default case on purpose: adding a DecodedEvent::Type without
        //deciding what it emits here must not compile silently.
        switch (ev.type)
        {
        case DecodedEvent::InputChange:
            emit eventInputChange(ev.id, ev.state, ev.value);
            break;
        case DecodedEvent::OutputChange:
            emit eventOutputChange(ev.id, ev.state, ev.value);
            break;
        case DecodedEvent::AudioVolumeChange:
            emit eventAudioVolumeChange(ev.id, ev.number);
            break;
        case DecodedEvent::AudioStatusChange:
            emit eventAudioStatusChange(ev.id, ev.state);
            break;
        case DecodedEvent::AudioSongChange:
            emit eventAudioChange(ev.id);
            break;
        case DecodedEvent::TouchscreenCamera:
            emit eventTouchscreenCamera(ev.id);
            break;
        case DecodedEvent::IoStatusChange:
            emit eventIoStatusChange(ev.id, ev.data);
            break;
        case DecodedEvent::InputStateChange:
            emit eventInputStateChange(ev.data);
            break;
        case DecodedEvent::OutputStateChange:
            emit eventOutputStateChange(ev.data);
            break;
        case DecodedEvent::AudioStateChange:
            emit eventAudioStateChange(ev.id, ev.data);
            break;
        case DecodedEvent::LogEvent:
            emit logEventLoaded(ev.data);
            break;

        case DecodedEvent::Unknown:
        case DecodedEvent::Malformed:
            //Event types the UI does not consume, and frames the decoder
            //refused. Both were already dropped before T17.
            break;
        }
    }
}

void CalaosConnection::processEventsV2(QString msg)
{
    if (msg == "") return;

#ifdef QT_DEBUG
    qDebug().noquote() << "Received: " << msg;
#endif

    dispatchEvents(CalaosEventDecoder::decodeEventV2(msg));
}

void CalaosConnection::processEventsV3(QVariantMap msg)
{
#ifdef QT_DEBUG
    //qDebug().noquote() << "Received: " << msg["event_raw"];
#endif

    dispatchEvents(CalaosEventDecoder::decodeEventV3(msg));
}

void CalaosConnection::onWsTextMessageReceived(const QString &message)
{
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(message.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError)
    {
        qWarning() << "JSON parse error at " << err.offset << " : " << err.errorString();

        if (reconnectPolicy.state() == ReconnectPolicy::Connecting)
            connectionLost(QStringLiteral("invalid JSON received while logging in"));

        return;
    }

    if (!jdoc.isObject())
    {
        qWarning() << "Invalid websocket frame, a JSON object was expected";

        if (reconnectPolicy.state() == ReconnectPolicy::Connecting)
            connectionLost(QStringLiteral("invalid websocket frame while logging in"));

        return;
    }

    QJsonObject jroot = jdoc.object();
    QJsonObject jdata = jroot[JsonKeys::Data].toObject();

#ifdef QT_DEBUG
    //qDebug() << "RECV:" << message;
#endif

    if (jroot[JsonKeys::Msg] == "login")
    {
        if (jdata[JsonKeys::Success] == "true")
        {
            if (constate != ConStateWebsocket)
            {
                constate = ConStateWebsocket;
                reconnectPolicy.attemptSucceeded();
                emitStateChanged();

                //ask for home
                sendWebsocket("get_home");
            }
        }
        else
        {
            //The server explicitly rejected the credentials. This used to be
            //ignored, leaving the application spinning on the loading screen
            //forever with a wrong password.
            connectionLost(QStringLiteral("server rejected the credentials"),
                           ReconnectPolicy::AuthFailure);
        }
    }
    else if (jroot[JsonKeys::Msg] == "get_home")
    {
        emitHomeLoaded(jroot[JsonKeys::Data].toObject().toVariantMap());
        HardwareUtils::Instance()->showNetworkActivity(false);
    }
    else if (jroot[JsonKeys::Msg] == "event")
    {
        processEventsV3(jroot[JsonKeys::Data].toObject().toVariantMap());
    }
    else if (jroot[JsonKeys::Msg] == "get_state")
    {
        //emit event for specific input/output change
        dispatchEvents(CalaosEventDecoder::decodeStateMap(jdata.toVariantMap(),
                                                          CalaosEventDecoder::StrictString));
    }
    else if (jroot[JsonKeys::Msg] == "eventlog")
    {
        emit logEventLoaded(jroot[JsonKeys::Data].toObject().toVariantMap());
    }
    else if (jroot[JsonKeys::Msg] == "settings")
    {
        if (jdata[JsonKeys::Action] == "change_cred")
        {
            if (jdata[JsonKeys::Success] == "true")
            {
                username = username_temp;
                password = password_temp;
                emit changeCredsSuccess(username, password);

                //reconnect
                logout();
                login(username, password, wshost);
            }
            else
            {
                emit changeCredsFailed();
            }
        }
    }

    //We get this marker when calling sendCommand(...) it helps disabling the net indicator
    if (jroot[JsonKeys::MsgId] == "user_cmd" || jroot[JsonKeys::MsgId] == "change_creds")
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
