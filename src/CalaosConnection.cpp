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
    //JsonKeys::Token is the push token sent by register_push: a per-device
    //credential as well (FIX-3).
    static const QStringList keys = { JsonKeys::CnUser,
                                      JsonKeys::CnPass,
                                      JsonKeys::Token,
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
    //The credentials travel in the body (and in the query string of the
    //picture urls): never replay them to another host.
    accessManager->setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    accessManagerCam->setRedirectPolicy(QNetworkRequest::SameOriginRedirectPolicy);
    pollReply = nullptr;
    connect(accessManager, &QNetworkAccessManager::sslErrors,
            this, &CalaosConnection::sslErrors);
    connect(accessManagerCam, &QNetworkAccessManager::sslErrors,
            this, &CalaosConnection::sslErrors);
    connect(pins(), &TlsPinStore::pinsChanged, this, &CalaosConnection::dropCachedConnections);
    connect(accessManager, &QNetworkAccessManager::encrypted, this, &CalaosConnection::encrypted);
    connect(accessManagerCam, &QNetworkAccessManager::encrypted, this, &CalaosConnection::encrypted);

    //A token reported while the session is up (rotation, or a token that only
    //arrived after the login) is registered right away, not at the next login.
    connect(HardwareUtils::Instance(), &HardwareUtils::deviceTokenChanged,
            this, &CalaosConnection::registerPushToken);
}

void CalaosConnection::sslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    //Synchronous on purpose: ignoreSslErrors() only counts from inside this
    //slot, so an unknown certificate fails the request and is asked about
    //afterwards.
    //These errors only, never all: the flag would stay on QNAM's cached
    //channel and let a later handshake with another certificate through.
    if (checkTls(reply->url(), reply->sslConfiguration().peerCertificate(), errors))
        reply->ignoreSslErrors(errors);
}

void CalaosConnection::sslErrorsWebsocket(const QList<QSslError> &errors)
{
    if (!wsocket)
        return;
    if (checkTls(wsocket->requestUrl(), wsocket->sslConfiguration().peerCertificate(), errors))
        wsocket->ignoreSslErrors(errors);
}

void CalaosConnection::encrypted(QNetworkReply *reply)
{
    //Emitted before any request data: a CA-valid certificate never reaches
    //sslErrors, so a pinned host's key is checked here too.
    if (!checkTls(reply->url(), reply->sslConfiguration().peerCertificate(), {}))
        reply->abort();
}

TlsPinStore *CalaosConnection::pins() const
{
    return pinStore ? pinStore : TlsPinStore::instance();
}

void CalaosConnection::setPinStore(TlsPinStore *store)
{
    disconnect(pins(), &TlsPinStore::pinsChanged, this, &CalaosConnection::dropCachedConnections);
    pinStore = store;
    connect(pins(), &TlsPinStore::pinsChanged, this, &CalaosConnection::dropCachedConnections);
}

void CalaosConnection::dropCachedConnections()
{
    //A forgotten pin must not survive in a kept-alive connection.
    accessManager->clearConnectionCache();
    accessManagerCam->clearConnectionCache();
}

bool CalaosConnection::checkTls(const QUrl &url, const QSslCertificate &peer, const QList<QSslError> &errors)
{
    const QSslCertificate cert = TlsPinning::peerCertificate(peer, errors);
    const QString key = TlsPinning::hostKey(url);
    const QString spki = TlsPinning::spkiSha256(cert);
    const QString stored = pins()->pin(key);
    const TlsPinning::Decision decision = TlsPinning::decide(errors, stored, spki);

    if (decision == TlsPinning::Decision::PinMatch || decision == TlsPinning::Decision::Trusted)
        return true;

    QStringList reasons;
    for (const QSslError &e: errors)
        reasons << e.errorString();

    //The attempt is over whatever happens below; the transport failure that
    //follows reports it as a TrustFailure. Queued for a request of a session
    //already up, whose failure would otherwise only be logged.
    tlsBlocked = true;
    QTimer::singleShot(0, this, [this]()
    {
        if (tlsBlocked)
            connectionLost(QStringLiteral("untrusted server certificate"), ReconnectPolicy::TrustFailure);
    });

    const QString refusalId = key + QLatin1Char('|') + spki;
    if (tlsRefused.contains(refusalId))
    {
        qInfo() << "TLS: certificate of" << key << "already refused in this run, not asking again";
        return false;
    }

    if (decision == TlsPinning::Decision::Refuse)
    {
        qWarning() << "TLS: certificate of" << key << "refused:" << reasons.join(QStringLiteral("; "));
        tlsRefused.insert(refusalId);
        emit tlsCertificateRefused(key, reasons.join(QStringLiteral("; ")), TlsPinning::isWeakRsaKey(cert));
        return false;
    }

    const bool keyChanged = decision == TlsPinning::Decision::AskKeyChanged;
    if (keyChanged)
        qWarning() << "TLS: key CHANGED for" << key << "SPKI sha256" << spki << "pinned" << stored
                   << "-" << reasons.join(QStringLiteral("; "));
    else
        qWarning() << "TLS: first certificate seen for" << key << "SPKI sha256" << spki
                   << "-" << reasons.join(QStringLiteral("; "));

    //One question at a time: every request of the attempt reports the same
    //certificate, and a different one meanwhile must not replace what the
    //user is reading. It is asked about at the next attempt.
    if (!pendingTrust.isEmpty())
    {
        if (pendingTrust.value(QStringLiteral("host")).toString() != key ||
            pendingTrust.value(QStringLiteral("spki")).toString() != spki)
            qInfo() << "TLS: a question is already pending, certificate of" << key << "refused for now";
        return false;
    }

    //Reached by DNS name, or a valid chain issued for another name: what a
    //man in the middle would present, so the dialog does not reassure.
    bool nameMismatchOnly = !errors.isEmpty();
    for (const QSslError &e: errors)
    {
        if (e.error() != QSslError::HostNameMismatch)
            nameMismatchOnly = false;
    }
    const bool suspicious = TlsPinning::isDnsName(url.host(QUrl::FullyEncoded)) || nameMismatchOnly;

    QVariantMap info;
    info[QStringLiteral("host")] = key;
    info[QStringLiteral("spki")] = spki;
    info[QStringLiteral("fingerprint")] = TlsPinning::formatFingerprint(spki);
    info[QStringLiteral("certFingerprint")] = TlsPinning::formatFingerprint(TlsPinning::certSha256(cert));
    info[QStringLiteral("subject")] = cert.subjectDisplayName();
    info[QStringLiteral("issuer")] = cert.issuerDisplayName();
    info[QStringLiteral("expiry")] = QLocale().toString(cert.expiryDate().date(), QLocale::ShortFormat);
    info[QStringLiteral("keyChanged")] = keyChanged;
    info[QStringLiteral("suspicious")] = suspicious;
    info[QStringLiteral("previousFingerprint")] = TlsPinning::formatFingerprint(stored);
    pendingTrust = info;
    emit tlsTrustRequested(info);
    return false;
}

void CalaosConnection::answerTlsTrust(bool accept)
{
    if (pendingTrust.isEmpty())
        return;

    const QVariantMap info = pendingTrust;
    pendingTrust.clear();
    const QString key = info.value(QStringLiteral("host")).toString();
    const QString spki = info.value(QStringLiteral("spki")).toString();

    if (!accept)
    {
        qInfo() << "TLS: user refused the certificate of" << key;
        tlsRefused.insert(key + QLatin1Char('|') + spki);
        emit tlsTrustRejected(key);
        return;
    }

    qInfo() << "TLS: user trusted" << key << "SPKI sha256" << spki;
    pins()->setPin(key, spki);
    tlsRefused.remove(key + QLatin1Char('|') + spki);

    //An explicit answer: start over now, like a tap on Login. Refused while
    //an attempt is in flight or a session is up, which then simply goes on.
    startConnection(username, password, lasthost, ReconnectPolicy::UserRequest);
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
    pushRegistration.sessionEnded();

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

    //Forgotten before the abort, which emits finished() synchronously: the
    //event log requests they carried are failed below, in one place, once
    //the transport is fully down.
    eventLogReplies.clear();

    const QList<QNetworkReply *> replies = reqReplies;
    reqReplies.clear();
    foreach (QNetworkReply *reply, replies)
    {
        reply->abort();
        reply->deleteLater();
    }

    //Last, so a listener sees a transport that is already down (EVLOG-1:
    //the answer was simply lost before and the event log waited forever).
    //Down means constate/isWebsocket()/isHttp() only: the callers update
    //reconnectPolicy, and emit connectionStateChanged(), after this returns,
    //so a listener must not read connectionState() here.
    failPendingEventLogRequests();
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

bool CalaosConnection::acceptsImmediateReconnect() const
{
    //Discovery fires once per announce datagram, so it can be chatty. It is
    //allowed to short-circuit the delay currently pending, but not more than
    //once per second: otherwise a talkative LAN would defeat the backoff.
    if (lastAttemptTimer.isValid() &&
        lastAttemptTimer.elapsed() < ReconnectPolicy::BaseDelayMs)
        return false;

    return reconnectPolicy.canBeginAttempt(ReconnectPolicy::AutoRetry);
}

void CalaosConnection::requestImmediateReconnect(QString user, QString pass, QString h)
{
    if (!acceptsImmediateReconnect())
        return;

    startConnection(user, pass, h, ReconnectPolicy::AutoRetry);
}

void CalaosConnection::suspend()
{
    cancelReconnectTimer();
    resetTransport();
    reconnectPolicy.suspend();
    emitStateChanged();
    emit disconnected();
}

void CalaosConnection::pause()
{
    cancelReconnectTimer();
    resetTransport();
    reconnectPolicy.pause();
    emitStateChanged();
    emit disconnected();
}

void CalaosConnection::unpause()
{
    reconnectPolicy.unpause();
}

void CalaosConnection::resume()
{
    //canResume() implies a session was wanted, hence that startConnection()
    //or deferLogin() (login refused offline, FIX-9) already ran and stored
    //the credentials and host.
    if (!reconnectPolicy.canResume())
        return;

    startConnection(username, password, lasthost, ReconnectPolicy::AutoRetry);
}

bool CalaosConnection::deferLogin(QString user, QString pass, QString h, ReconnectPolicy::Trigger trigger)
{
    if (!reconnectPolicy.deferAttempt(trigger))
        return false;

    //Nothing is in flight (deferAttempt() refuses otherwise), but a retry may
    //be pending: the new intent replaces it, resume() starts the attempt.
    cancelReconnectTimer();
    resetTransport();

    //What resume() will use, like an attempt started now would have stored.
    username = user;
    password = pass;
    lasthost = h;

    emitStateChanged();
    return true;
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

    //A question left open belongs to the previous attempt (maybe another
    //host): it must neither block this one nor be answered for it.
    if (!pendingTrust.isEmpty())
    {
        pendingTrust.clear();
        emit tlsTrustCanceled();
    }

    //Tear the previous transport down first: aborting its replies can report
    //failures synchronously, and those must land while the policy still knows
    //about the old attempt.
    resetTransport();
    cancelReconnectTimer();
    reconnectPolicy.beginAttempt(trigger);
    tlsBlocked = false;
    lastAttemptTimer.restart();
    emitStateChanged();

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
    //A CA-valid certificate never reaches sslErrorsWebsocket(): check the
    //pinned key before the login frame carries the credentials.
    if (wsocket->requestUrl().scheme() == QLatin1String("wss") &&
        !checkTls(wsocket->requestUrl(), wsocket->sslConfiguration().peerCertificate(), {}))
    {
        handleWebsocketFailure(QStringLiteral("untrusted server certificate"));
        return;
    }

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

    //register_push is sent once the login is accepted (see the "login"
    //answer in onWsTextMessageReceived()): sendWebsocket() drops anything
    //sent before that, which used to lose the registration on a slow link.
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

    //The HTTP endpoint is the same server with the same certificate: falling
    //back would only ask the same question twice.
    if (tlsBlocked)
    {
        connectionLost(reason, ReconnectPolicy::TrustFailure);
        return;
    }

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
    cancelReconnectTimer();
    resetTransport();
    //Answering after a logout would log in again behind the user's back.
    if (!pendingTrust.isEmpty())
    {
        pendingTrust.clear();
        emit tlsTrustCanceled();
    }
    //Explicit logout: no automatic reconnection is wanted any more, and none
    //of suspend()/resume(), discovery or the retry timer may reopen it.
    reconnectPolicy.stop();
    emitStateChanged();

    emit disconnected();
}

void CalaosConnection::loginFinished(QNetworkReply *reply)
{
    disconnect(accessManager, &QNetworkAccessManager::finished,
               this, &CalaosConnection::loginFinished);

    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reply->url()) << ":" << reply->errorString();

        if (tlsBlocked)
        {
            connectionLost(reply->errorString(), ReconnectPolicy::TrustFailure);
            return;
        }

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

    //Register the push token of this device for this session, if known yet.
    pushRegistration.sessionStarted();
    registerPushToken();

    //start polling
    startJsonPolling();

    emitHomeLoaded(jroot);
}

void CalaosConnection::registerPushToken()
{
    //Nothing to send on: the next login registers the current token.
    if (!isWebsocket() && !isHttp())
        return;

    const QString token = HardwareUtils::Instance()->getDeviceToken();
    if (!pushRegistration.shouldRegister(token))
        return;

#if defined(Q_OS_ANDROID)
    const QString hw = PushFormat::AndroidHardware;
#elif defined(Q_OS_IOS)
    const QString hw = QStringLiteral("ios");
#else
    const QString hw;
#endif

    qDebug() << "Registering push token" << HardwareUtils::describeToken(token)
             << "for" << (hw.isEmpty() ? QStringLiteral("<unknown>") : hw);

    //Send the push device token to register the mobile device to calaos_server
    //for push notif (Android also asks for data-only messages, see PushFormat).
    QJsonObject o = PushFormat::registration(token, hw,
                                             HardwareUtils::Instance()->getDeviceName());

    if (isWebsocket())
    {
        sendWebsocket(QStringLiteral("register_push"), o);
    }
    else
    {
        //api.php checks the credentials before looking at the action, like
        //for every other HTTP request: without them the server answers 400
        //and the device is never registered. Redacted in the debug dump.
        o[JsonKeys::CnUser] = username;
        o[JsonKeys::CnPass] = password;
        sendHttp(QStringLiteral("register_push"), o);
    }
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
    QNetworkReply *reqReply = qobject_cast<QNetworkReply*>(QObject::sender());

    if (!reqReply)
    {
        qWarning() << "Error reqReply is NULL!";
        return;
    }

    reqReply->deleteLater();
    //Forget it on every path: resetTransport() aborts what is still listed,
    //and a failed reply left here was aborted after its deletion (crash on
    //the next logout or reconnection once any HTTP request had failed).
    reqReplies.removeAll(reqReply);

    //An event log request: answered (or failed) on every path, see
    //eventLogReplyFinished().
    const QString eventLogId = eventLogReplies.take(reqReply);
    if (!eventLogId.isEmpty())
    {
        eventLogReplyFinished(reqReply, eventLogId);
        return;
    }

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

    QVariantMap jroot = jdoc.object().toVariantMap();

    //Audio player states and io states can be in the same answer: the
    //decoder reports every payload it recognises, in order. Event log answers
    //never get here, they are tied to their request above.
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
    //Forget it on every path, see requestFinished(): a failed picture is
    //common, and left listed it was aborted after its deletion by the next
    //resetTransport() (logout, reconnection, iOS going to the background).
    reqReplies.removeAll(reqReply);

    if (reqReply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reqReply->url()) << ":" << reqReply->errorString();
        emit cameraPictureFailed(camid);
        return;
    }

    QByteArray bytes = reqReply->readAll();

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
    //Forget it on every path, see requestFinished(): a failed picture is
    //common, and left listed it was aborted after its deletion by the next
    //resetTransport() (logout, reconnection, iOS going to the background).
    reqReplies.removeAll(reqReply);

    if (reqReply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reqReply->url()) << ":" << reqReply->errorString();
        return;
    }

    QByteArray bytes = reqReply->readAll();

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

bool CalaosConnection::sendWebsocket(const QString &msg, const QJsonObject &data, const QString &client_id)
{
    if (!isWebsocket()) return false;

    QJsonObject o = {{ JsonKeys::Msg, msg },
                     { JsonKeys::Data, data }};
    if (!client_id.isEmpty())
        o[JsonKeys::MsgId] = client_id;

    QJsonDocument doc(o);
#ifdef QT_DEBUG
    //qDebug().noquote() << "SEND: " << doc.toJson();
#endif

    wsocket->sendTextMessage(doc.toJson());
    return true;
}

QNetworkReply *CalaosConnection::sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors)
{
    if (!isHttp()) return nullptr;

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
    return reqReply;
}

void CalaosConnection::sendCommand(QString id, QString value, QString type, QString action)
{
    sendNumberedCommand(id, value, type, action, 0);
}

void CalaosConnection::sendNumberedCommand(QString id, QString value, QString type, QString action, int seq)
{
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
        sendWebsocket(action, jroot,
                      action == QLatin1String("set_state")
                          ? JsonKeys::SetStateTag + id + (seq > 0 ? JsonKeys::SetStateSeqSeparator + QString::number(seq) : QString())
                          : QStringLiteral("user_cmd"));
    else
        sendHttp(action, jroot);
}

bool CalaosConnection::sendJson(const QString &action, QJsonObject &jsonData, const QString &msgId)
{
    if (isWebsocket())
        return sendWebsocket(action, jsonData, msgId);

    if (isHttp())
    {
        jsonData[JsonKeys::CnUser] = username;
        jsonData[JsonKeys::CnPass] = password;

        return sendHttp(action, jsonData) != nullptr;
    }

    //No session up: nothing is sent, and the caller must not wait for an
    //answer that cannot come.
    return false;
}

//msg_id prefixes of the event log requests. calaos_server echoes the msg_id
//of every eventlog request in its answer (JsonApiHandlerWS::processEventLog),
//which is what ties an answer to its request. Values, not keys: they stay
//here and not in JsonKeys.h.
static const QLatin1StringView EventLogPageTag{"eventlog_page:"};
static const QLatin1StringView EventLogEventTag{"eventlog_event:"};

QString CalaosConnection::requestEventLogPage(int page, int perPage)
{
    return requestEventLog(EventLogKind::Page,
                           QJsonObject{{ JsonKeys::Page, page },
                                       { JsonKeys::PerPage, perPage }});
}

QString CalaosConnection::requestEventLogEvent(const QString &uuid)
{
    return requestEventLog(EventLogKind::Event, QJsonObject{{ JsonKeys::Uuid, uuid }});
}

QString CalaosConnection::requestEventLog(EventLogKind kind, QJsonObject data)
{
    const QString id = (kind == EventLogKind::Page ? EventLogPageTag : EventLogEventTag) +
                       QString::number(++eventLogSeq);

    if (isWebsocket())
    {
        if (!sendWebsocket(QStringLiteral("eventlog"), data, id))
            return QString();
    }
    else if (isHttp())
    {
        //The HTTP answer is tied to its request by the reply that carries it.
        data[JsonKeys::CnUser] = username;
        data[JsonKeys::CnPass] = password;
        QNetworkReply *reply = sendHttp(QStringLiteral("eventlog"), data);
        if (!reply)
            return QString();
        eventLogReplies.insert(reply, id);
    }
    else
    {
        qDebug() << "Event log request not sent: no session up";
        return QString();
    }

    pendingEventLog.insert(id, kind);
    return id;
}

void CalaosConnection::cancelEventLogRequest(const QString &requestId)
{
    pendingEventLog.remove(requestId);
    //The HTTP reply, if any, finishes on its own; requestFinished() then
    //finds no request waiting for it.
}

void CalaosConnection::finishEventLogRequest(const QString &requestId, bool transportOk, const QVariantMap &answer)
{
    const auto it = pendingEventLog.constFind(requestId);
    if (it == pendingEventLog.constEnd())
    {
        //Finished already, cancelled after a timeout, or sent by an older
        //session: nobody waits for it any more.
        qDebug() << "Event log answer dropped, no request waits for" << requestId;
        return;
    }

    const EventLogKind kind = it.value();
    pendingEventLog.erase(it);

    //A server that could not read the log answers {"error": ...} instead:
    //that ends the request just as well.
    const bool serverError = answer.contains(JsonKeys::Error);
    if (kind == EventLogKind::Page)
    {
        const bool ok = transportOk && !serverError && answer.contains(JsonKeys::Events);
        if (!ok)
            qWarning() << "Event log page request" << requestId << "failed:"
                       << (transportOk ? answer.value(JsonKeys::Error).toString() : QStringLiteral("transport"));
        emit eventLogPageFinished(requestId, ok, answer);
    }
    else
    {
        const bool ok = transportOk && !serverError && !answer.isEmpty();
        emit eventLogEventFinished(requestId, ok, answer);
    }
}

void CalaosConnection::failPendingEventLogRequests()
{
    //Ids are unique, so the order does not matter; sorted only to make the
    //sequence of signals reproducible.
    QStringList ids = pendingEventLog.keys();
    ids.sort();
    for (const QString &id: std::as_const(ids))
        finishEventLogRequest(id, false, QVariantMap());
}

void CalaosConnection::eventLogReplyFinished(QNetworkReply *reply, const QString &requestId)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "Error in " << redactUrl(reply->url()) << ":" << reply->errorString();
        finishEventLogRequest(requestId, false, QVariantMap());
        return;
    }

    const QByteArray bytes = reply->readAll();
    QJsonParseError err;
    const QJsonDocument jdoc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !jdoc.isObject())
    {
        qDebug() << "Event log answer: JSON parse error at " << err.offset << " : " << err.errorString();
        finishEventLogRequest(requestId, false, QVariantMap());
        return;
    }

    const QVariantMap answer = jdoc.object().toVariantMap();

    //A page is the LogEvent the decoder finds in the answer (it reads the
    //top level shape api.php sends). A single event, or an error, is the
    //answer itself.
    QVariantMap payload = answer;
    if (pendingEventLog.value(requestId) == EventLogKind::Page)
    {
        const QList<DecodedEvent> events = CalaosEventDecoder::decodeQueryAnswer(
                    answer, isHttpApiV2() ? CalaosEventDecoder::ApiV2 : CalaosEventDecoder::ApiV3);
        for (const DecodedEvent &ev: events)
        {
            if (ev.type == DecodedEvent::LogEvent)
            {
                payload = ev.data;
                break;
            }
        }
    }

    finishEventLogRequest(requestId, true, payload);
}

void CalaosConnection::queryState(QStringList inputs, QStringList outputs, QStringList audio_players)
{
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
        case DecodedEvent::DeviceChange:
            emit eventDeviceChange(ev.id, ev.data);
            break;
        case DecodedEvent::CommandRefused:
            emit eventCommandRefused(ev.id);
            break;
        case DecodedEvent::CommandAnswered:
            emit eventCommandAnswered(ev.id, ev.value.toInt(), ev.state == QLatin1String("true"));
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
            //Event log pages are tied to their request instead, see
            //eventLogReplyFinished(): a page nobody asked for is dropped.
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

                //Register the push token of this device for this session,
                //if known yet.
                pushRegistration.sessionStarted();
                registerPushToken();
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
    else if (jroot[JsonKeys::Msg] == "set_state")
    {
        dispatchEvents({ CalaosEventDecoder::decodeSetStateAnswer(jroot.toVariantMap()) });
    }
    else if (jroot[JsonKeys::Msg] == "eventlog")
    {
        //calaos_server echoes the msg_id of the request: the answer goes to
        //that request only (EVLOG-1), a page and a single event can no
        //longer be taken for one another, nor a late answer for a newer one.
        finishEventLogRequest(jroot[JsonKeys::MsgId].toString(), true, jdata.toVariantMap());
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
}

QString CalaosConnection::getNotifPictureUrl(const QString &pic_uid)
{
    return QStringLiteral("%1?cn_user=%2&cn_pass=%3&action=event_picture&pic_uid=%4")
            .arg(httphost)
            .arg(username)
            .arg(password)
            .arg(pic_uid);
}
