/*
 * Push notification plumbing shared by iOS and Android (FIX-3).
 *
 * Bugs covered:
 *  1. A notification tapped while the session was down (reconnection) was
 *     lost: the deferred tap was only consumed on the very first login of the
 *     process. HardwareUtils now keeps it, and Application opens it once at
 *     the next homeLoaded() through DeferredNotifGate. The gate is driven here
 *     exactly the way Application drives it (see FakeApp), timers aside.
 *  2. A rotated push token only reached calaos_server at the next login.
 *     HardwareUtils emits deviceTokenChanged() and CalaosConnection sends
 *     register_push again while the session is up, never twice for the same
 *     token in one session (PushRegistration). Over HTTP the request carries
 *     the credentials, which calaos_server checks before any action.
 *  3. The push token was dumped in clear by the debug log of the HTTP
 *     fallback (sendHttp()): "token" is now a redacted key.
 *
 * register_push also carries the device name the admin chooses the approver
 * phone by, over both transports.
 *
 * The CalaosConnection cases run a real connection against in-process fake
 * servers (HTTP api.php, websocket /api, and TLS for the websocket -> HTTP
 * fallback): no calaos_server, no network outside the loopback.
 *
 * Every test function sets up its own token and can run alone.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include "CalaosConnection.h"
#include "HardwareUtils.h"

static const QString TestUser = QStringLiteral("user");
static const QString TestPass = QStringLiteral("s3cret-pass");

//Every log line of the process, to check that no push token leaks in clear.
static QStringList s_logLines;
static QtMessageHandler s_previousHandler = nullptr;
//register_push accepted by an HTTP fake during the run (any test).
static int s_httpRegistrations = 0;

static void captureMessages(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    s_logLines << msg;
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}

/*
 * Plays calaos_server's api.php: like the real one (JsonApiHandlerHttp), any
 * request without the right cn_user/cn_pass is answered 400 before the action
 * is even looked at. Answers the get_home login, records every register_push,
 * hands out a polling uuid and then holds the long-poll open (a quiet
 * installation), so the session stays up without any traffic. Speaks TLS when
 * given a configuration.
 */
class FakeHttpServer: public QTcpServer
{
    Q_OBJECT

public:
    explicit FakeHttpServer(const QSslConfiguration *tls = nullptr):
        tlsConfig(tls ? *tls : QSslConfiguration()),
        useTls(tls != nullptr)
    {
    }

    //Delay before the login (get_home) is answered.
    int loginDelayMs = 0;
    int logins = 0;
    //register_push requests that passed the credentials check.
    QList<QJsonObject> registrations;
    //Actions refused for missing/wrong credentials.
    QStringList rejected;
    //Plain GET requests (a websocket upgrade attempt), refused with 400.
    int getRequests = 0;

    QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1/api.php").arg(serverPort());
    }

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = nullptr;
        if (useTls)
        {
            QSslSocket *ssl = new QSslSocket(this);
            ssl->setSslConfiguration(tlsConfig);
            ssl->setSocketDescriptor(handle);
            ssl->startServerEncryption();
            sock = ssl;
        }
        else
        {
            sock = new QTcpSocket(this);
            sock->setSocketDescriptor(handle);
        }

        QByteArray *buf = new QByteArray();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf]()
        {
            buf->append(sock->readAll());
            const int hdrEnd = buf->indexOf("\r\n\r\n");
            if (hdrEnd < 0)
                return;
            const QByteArray hdr = buf->left(hdrEnd);
            int contentLength = 0;
            for (const QByteArray &line: hdr.split('\n'))
            {
                if (line.toLower().startsWith("content-length:"))
                    contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
            const QByteArray body = buf->mid(hdrEnd + 4);
            if (body.size() < contentLength)
                return;
            buf->clear();

            if (hdr.startsWith("GET "))
            {
                getRequests++;
                reply(sock, 400, "text/html", "<html>Bad request</html>");
                return;
            }
            handleRequest(sock, QJsonDocument::fromJson(body).object());
        });
        connect(sock, &QTcpSocket::disconnected, this, [sock, buf]()
        {
            delete buf;
            sock->deleteLater();
        });
    }

private:
    void handleRequest(QTcpSocket *sock, const QJsonObject &j)
    {
        const QString action = j.value(QStringLiteral("action")).toString();

        if (j.value(QStringLiteral("cn_user")).toString() != TestUser ||
            j.value(QStringLiteral("cn_pass")).toString() != TestPass)
        {
            rejected << action;
            reply(sock, 400, "text/html", "<html>Bad request</html>");
            return;
        }

        if (action == QLatin1String("get_home"))
        {
            logins++;
            QTimer::singleShot(loginDelayMs, sock, [sock]()
            {
                reply(sock, 200, "application/json",
                      "{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}");
            });
            return;
        }

        if (action == QLatin1String("register_push"))
        {
            registrations << j;
            s_httpRegistrations++;
            reply(sock, 200, "application/json", "{\"success\":\"true\"}");
            return;
        }

        if (action == QLatin1String("poll_listen"))
        {
            if (j.value(QStringLiteral("type")).toString() == QLatin1String("register"))
                reply(sock, 200, "application/json", "{\"success\":\"true\",\"uuid\":\"fake-poll-uuid\"}");
            //"get": never answered, like a long-poll with no event.
            return;
        }

        reply(sock, 200, "application/json", "{\"success\":\"true\"}");
    }

    static void reply(QTcpSocket *sock, int code, const char *mime, const QByteArray &body)
    {
        if (sock->state() != QAbstractSocket::ConnectedState)
            return;
        QByteArray out = "HTTP/1.1 " + QByteArray::number(code)
                         + (code == 200 ? " OK" : " Bad Request")
                         + "\r\nContent-Type: " + mime;
        out += "\r\nContent-Length: " + QByteArray::number(body.size());
        out += "\r\nConnection: close\r\n\r\n";
        out += body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }

    QSslConfiguration tlsConfig;
    bool useTls = false;
};

/*
 * Plays calaos_server's websocket API: accepts the login after loginDelayMs,
 * records every register_push and the order of the messages received.
 */
class FakeWsServer: public QWebSocketServer
{
    Q_OBJECT

public:
    FakeWsServer():
        QWebSocketServer(QStringLiteral("fake calaos_server"), QWebSocketServer::NonSecureMode)
    {
        connect(this, &QWebSocketServer::newConnection, this, [this]()
        {
            while (QWebSocket *ws = nextPendingConnection())
            {
                ws->setParent(this);
                connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString &m)
                {
                    handleMessage(ws, QJsonDocument::fromJson(m.toUtf8()).object());
                });
            }
        });
    }

    int loginDelayMs = 0;
    QStringList received; //msg of every frame, in order
    QList<QJsonObject> registrations; //"data" of every register_push

    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1/api").arg(serverPort());
    }

private:
    void handleMessage(QWebSocket *ws, const QJsonObject &j)
    {
        const QString msg = j.value(QStringLiteral("msg")).toString();
        received << msg;

        if (msg == QLatin1String("login"))
        {
            QTimer::singleShot(loginDelayMs, ws, [ws]()
            {
                ws->sendTextMessage(QStringLiteral(
                    "{\"msg\":\"login\",\"data\":{\"success\":\"true\"}}"));
            });
        }
        else if (msg == QLatin1String("get_home"))
        {
            ws->sendTextMessage(QStringLiteral(
                "{\"msg\":\"get_home\",\"data\":{\"home\":[],\"cameras\":[],\"audio\":[]}}"));
        }
        else if (msg == QLatin1String("register_push"))
        {
            registrations << j.value(QStringLiteral("data")).toObject();
        }
    }
};

/*
 * What Application does with the deferred tap, minus the QTimer and the QML
 * dispatch: homeLoaded() (take + gate + "connected"), the 400 ms delay
 * expiring (fire) and stopPushNotifRouting() (session down).
 */
struct FakeApp
{
    DeferredNotifGate gate;

    quint64 homeLoaded()
    {
        const quint64 ticket = gate.loggedIn(HardwareUtils::Instance()->takePendingNotif());
        HardwareUtils::Instance()->updateCalaosConnectState(true);
        return ticket;
    }

    QString delayExpired(quint64 ticket) { return gate.fire(ticket); }

    void sessionDown()
    {
        HardwareUtils::Instance()->updateCalaosConnectState(false);
        HardwareUtils::Instance()->restorePendingNotif(gate.loggedOut());
    }

    //Application::userLogout(): logout() (the session goes down as above),
    //then what the user asked of that session is forgotten (FIX-7).
    void userLogout()
    {
        sessionDown();
        HardwareUtils::Instance()->forgetPendingUserActions();
    }

    //Application::resetAllData(): host and credentials wiped (not modelled
    //here), logout(), then the same forgetting as userLogout() (FIX-7).
    void resetAllData()
    {
        sessionDown();
        HardwareUtils::Instance()->forgetPendingUserActions();
    }
};

/*
 * The base class with a way to fake an iOS Quick Action launch: startedWithOpt
 * is only set by platform code.
 */
class StartOptionHw: public HardwareUtils
{
public:
    StartOptionHw(): HardwareUtils(nullptr) {}
    void startWithOption() { startedWithOpt = true; }
};

class TstPushNotif: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    //PushRegistration alone.
    void registrationNeedsASession();
    void registrationOncePerTokenAndSession();
    void registrationRotationWhileUp();
    void registrationNewSessionRegistersAgain();
    void registrationIgnoresEmptyToken();

    //HardwareUtils: deferred notification tap.
    void tapWhileDisconnectedIsDeferred();
    void deferredTapIsTakenOnce();
    void latestDeferredTapWins();
    void tapWhileConnectedIsOpenedNow();
    void emptyTapIsIgnored();
    void restoredTapLosesToANewerOne();

    //DeferredNotifGate, alone and driven like Application drives it.
    void gateNothingToOpen();
    void gateOpensOnceAfterTheDelay();
    void gateStaleDelayNeverOpensAfterANewLogin();
    void gateNewerTapReplacesTheOneInFlight();
    void tapDuringEveryReconnectionIsOpened();
    void tapTakenButSessionDroppedOpensAtNextLogin();

    //FIX-7: an explicit user logout forgets what was asked of that session.
    void tapPendingThenUserLogoutOpensNothing();
    void tapInFlightThenUserLogoutOpensNothing();
    void tapPendingThenLoginCancelStillOpens();
    void tapAfterUserLogoutOpensAtNextLogin();
    void userLogoutForgetsTheStartOption();
    void resetAllDataOpensNothing();

    //HardwareUtils: push token.
    void tokenChangeIsSignalledOnce();
    void clearedTokenIsSignalled();
    void describedTokenIsTruncated();

    //register_push payload (AND-7): Android asks for data-only FCM messages,
    //no other hardware does. The connection tests below also check that this
    //host build (hardware unknown) never sends the field.
    void payloadAsksDataFormatForAndroid();
    void payloadHasNoFormatForOtherHardware();
    void payloadCarriesTheDeviceName();
    void payloadWithoutDeviceNameOmitsTheField();

    //CalaosConnection over HTTP (the transport whose debug log leaked).
    void httpRegistersAtLogin();
    void httpRotationWhileConnectedRegistersAgain();
    void httpTokenArrivingAfterLoginIsRegistered();
    void httpTokenChangeDuringLoginIsSentOnce();
    void httpTokenChangeWhileLoggedOutWaitsForLogin();
    void httpFallbackFromWebsocketRegistersOnce();

    //CalaosConnection over websocket.
    void wsRegistersAfterTheLoginIsAccepted();
    void wsRotationWhileConnectedRegistersAgain();

    //No token in clear in the logs, whatever ran above.
    void cleanupTestCase();

private:
    static HardwareUtils *hw() { return HardwareUtils::Instance(); }
    static void pump(int ms);
    //A token never used before in this run, recorded to scan the logs.
    static QString freshToken();
    static QStringList &usedTokens();
    //The register_push of an HTTP fake carries the right credentials.
    static void verifyAuthenticated(const QJsonObject &registration);
    //The name the base HardwareUtils reports, sent next to the token.
    static void verifyDeviceName(const QJsonObject &registration);

    void startConnection(const QString &host);

    FakeHttpServer *http = nullptr;
    FakeWsServer *ws = nullptr;
    CalaosConnection *connection = nullptr;

    QTemporaryDir certDir;
    QSslConfiguration tlsConfig;
    bool tlsReady = false;
};

void TstPushNotif::initTestCase()
{
    s_previousHandler = qInstallMessageHandler(captureMessages);

    //TLS for the websocket -> HTTP fallback case only: the mobile build
    //derives wss:// and https:// from a bare host. A throwaway self-signed
    //certificate is generated for the run (no key in the repository); the
    //OpenSSL backend is preferred for its PEM support in server mode.
    if (QSslSocket::availableBackends().contains(QStringLiteral("openssl")))
        QSslSocket::setActiveBackend(QStringLiteral("openssl"));
    if (!QSslSocket::supportsSsl() || !certDir.isValid())
        return;

    const QString key = certDir.filePath(QStringLiteral("key.pem"));
    const QString crt = certDir.filePath(QStringLiteral("crt.pem"));
    QProcess openssl;
    openssl.start(QStringLiteral("openssl"),
                  { QStringLiteral("req"), QStringLiteral("-x509"),
                    QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
                    QStringLiteral("-nodes"), QStringLiteral("-days"), QStringLiteral("1"),
                    QStringLiteral("-subj"), QStringLiteral("/CN=127.0.0.1"),
                    QStringLiteral("-keyout"), key, QStringLiteral("-out"), crt });
    if (!openssl.waitForFinished(30000) || openssl.exitStatus() != QProcess::NormalExit ||
        openssl.exitCode() != 0)
        return;

    QFile kf(key), cf(crt);
    if (!kf.open(QIODevice::ReadOnly) || !cf.open(QIODevice::ReadOnly))
        return;
    const QSslKey sslKey(kf.readAll(), QSsl::Rsa, QSsl::Pem);
    const QList<QSslCertificate> certs = QSslCertificate::fromData(cf.readAll(), QSsl::Pem);
    if (sslKey.isNull() || certs.isEmpty())
        return;

    tlsConfig = QSslConfiguration::defaultConfiguration();
    tlsConfig.setLocalCertificate(certs.first());
    tlsConfig.setPrivateKey(sslKey);
    tlsConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    tlsReady = true;
}

void TstPushNotif::init()
{
    //Every case starts logged out, with no deferred tap and no token.
    hw()->updateCalaosConnectState(false);
    hw()->takePendingNotif();
    hw()->clearDeviceToken();
}

void TstPushNotif::cleanup()
{
    if (connection)
    {
        connection->logout();
        pump(50);
        delete connection;
        connection = nullptr;
    }
    delete http;
    http = nullptr;
    delete ws;
    ws = nullptr;
}

void TstPushNotif::pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

QStringList &TstPushNotif::usedTokens()
{
    static QStringList tokens;
    return tokens;
}

QString TstPushNotif::freshToken()
{
    //Long and recognisable, like a real FCM/APNs token.
    static int n = 0;
    const QString t = QStringLiteral("fcmTok%1_").arg(++n, 3, 10, QLatin1Char('0'))
                      + QString(140, QLatin1Char('x'));
    usedTokens() << t;
    return t;
}

void TstPushNotif::verifyAuthenticated(const QJsonObject &registration)
{
    QCOMPARE(registration.value(QStringLiteral("cn_user")).toString(), TestUser);
    QCOMPARE(registration.value(QStringLiteral("cn_pass")).toString(), TestPass);
    //Not an Android build: no push_format (see payloadAsksDataFormatForAndroid).
    QVERIFY(!registration.contains(QStringLiteral("push_format")));
    //At the root next to the credentials, where api.php reads every argument.
    verifyDeviceName(registration);
}

void TstPushNotif::verifyDeviceName(const QJsonObject &registration)
{
    const QString expected = QSysInfo::machineHostName().trimmed();
    if (expected.isEmpty())
        QVERIFY(!registration.contains(QStringLiteral("device_name")));
    else
        QCOMPARE(registration.value(QStringLiteral("device_name")).toString(), expected);
}

void TstPushNotif::startConnection(const QString &host)
{
    connection = new CalaosConnection();
    connection->login(TestUser, TestPass, host);
}

// ---------------------------------------------------------------- PushRegistration

void TstPushNotif::registrationNeedsASession()
{
    PushRegistration r;
    QVERIFY(!r.isSessionUp());
    QVERIFY(!r.shouldRegister(QStringLiteral("tok")));

    r.sessionStarted();
    r.sessionEnded();
    QVERIFY(!r.shouldRegister(QStringLiteral("tok")));
}

void TstPushNotif::registrationOncePerTokenAndSession()
{
    PushRegistration r;
    r.sessionStarted();
    QVERIFY(r.shouldRegister(QStringLiteral("tok")));
    QCOMPARE(r.registeredToken(), QStringLiteral("tok"));
    //Login path and token-change path both asking: only one send.
    QVERIFY(!r.shouldRegister(QStringLiteral("tok")));
}

void TstPushNotif::registrationRotationWhileUp()
{
    PushRegistration r;
    r.sessionStarted();
    QVERIFY(r.shouldRegister(QStringLiteral("old")));
    QVERIFY(r.shouldRegister(QStringLiteral("new")));
    QCOMPARE(r.registeredToken(), QStringLiteral("new"));
    QVERIFY(!r.shouldRegister(QStringLiteral("new")));
}

void TstPushNotif::registrationNewSessionRegistersAgain()
{
    PushRegistration r;
    r.sessionStarted();
    QVERIFY(r.shouldRegister(QStringLiteral("tok")));
    r.sessionEnded();
    QVERIFY(r.registeredToken().isEmpty());
    //The server may have dropped it (new install, restart): always sent once
    //per session.
    r.sessionStarted();
    QVERIFY(r.shouldRegister(QStringLiteral("tok")));
}

void TstPushNotif::registrationIgnoresEmptyToken()
{
    PushRegistration r;
    r.sessionStarted();
    QVERIFY(!r.shouldRegister(QString()));
    QVERIFY(r.shouldRegister(QStringLiteral("tok")));
}

// ---------------------------------------------------------------- deferred tap

void TstPushNotif::tapWhileDisconnectedIsDeferred()
{
    QSignalSpy opened(hw(), &HardwareUtils::pushNotifReceived);

    hw()->handleNotificationOpened(QStringLiteral("uuid-1"));

    QCOMPARE(opened.count(), 0);
    QVERIFY(hw()->hasPendingNotif());
}

void TstPushNotif::deferredTapIsTakenOnce()
{
    hw()->handleNotificationOpened(QStringLiteral("uuid-1"));

    //First homeLoaded() after the reconnection.
    QCOMPARE(hw()->takePendingNotif(), QStringLiteral("uuid-1"));
    QVERIFY(!hw()->hasPendingNotif());

    //Any later homeLoaded() of the same process must not reopen it.
    QVERIFY(hw()->takePendingNotif().isEmpty());
}

void TstPushNotif::latestDeferredTapWins()
{
    hw()->handleNotificationOpened(QStringLiteral("uuid-old"));
    hw()->handleNotificationOpened(QStringLiteral("uuid-new"));

    QCOMPARE(hw()->takePendingNotif(), QStringLiteral("uuid-new"));
    QVERIFY(hw()->takePendingNotif().isEmpty());
}

void TstPushNotif::tapWhileConnectedIsOpenedNow()
{
    hw()->handleNotificationOpened(QStringLiteral("uuid-stale"));
    hw()->updateCalaosConnectState(true);

    QSignalSpy opened(hw(), &HardwareUtils::pushNotifReceived);
    hw()->handleNotificationOpened(QStringLiteral("uuid-now"));

    QCOMPARE(opened.count(), 1);
    QCOMPARE(opened.at(0).at(0).toString(), QStringLiteral("uuid-now"));
    //The tap being opened supersedes an older deferred one.
    QVERIFY(!hw()->hasPendingNotif());
}

void TstPushNotif::emptyTapIsIgnored()
{
    QSignalSpy opened(hw(), &HardwareUtils::pushNotifReceived);

    hw()->handleNotificationOpened(QString());
    hw()->updateCalaosConnectState(true);
    hw()->handleNotificationOpened(QString());

    QCOMPARE(opened.count(), 0);
    QVERIFY(!hw()->hasPendingNotif());
}

void TstPushNotif::restoredTapLosesToANewerOne()
{
    hw()->restorePendingNotif(QString());
    QVERIFY(!hw()->hasPendingNotif());

    hw()->restorePendingNotif(QStringLiteral("uuid-restored"));
    QCOMPARE(hw()->takePendingNotif(), QStringLiteral("uuid-restored"));

    hw()->handleNotificationOpened(QStringLiteral("uuid-newer"));
    hw()->restorePendingNotif(QStringLiteral("uuid-restored"));
    QCOMPARE(hw()->takePendingNotif(), QStringLiteral("uuid-newer"));
}

// ---------------------------------------------------------------- DeferredNotifGate

void TstPushNotif::gateNothingToOpen()
{
    DeferredNotifGate g;
    QCOMPARE(g.loggedIn(QString()), quint64(0));
    QVERIFY(g.fire(0).isEmpty());
    QVERIFY(g.loggedOut().isEmpty());
}

void TstPushNotif::gateOpensOnceAfterTheDelay()
{
    DeferredNotifGate g;
    const quint64 t = g.loggedIn(QStringLiteral("uuid-1"));
    QVERIFY(t != 0);
    QCOMPARE(g.fire(t), QStringLiteral("uuid-1"));
    QVERIFY(g.fire(t).isEmpty());
    //Already opened: nothing to hand back when the session drops.
    QVERIFY(g.loggedOut().isEmpty());
}

void TstPushNotif::gateStaleDelayNeverOpensAfterANewLogin()
{
    DeferredNotifGate g;
    const quint64 t1 = g.loggedIn(QStringLiteral("uuid-1"));
    //The session drops before the delay expires: the uuid is handed back...
    QCOMPARE(g.loggedOut(), QStringLiteral("uuid-1"));
    //...and the next login takes it with a new ticket.
    const quint64 t2 = g.loggedIn(QStringLiteral("uuid-1"));
    QVERIFY(t2 != t1);

    //The delay of the first login fires during the second one: void.
    QVERIFY(g.fire(t1).isEmpty());
    QCOMPARE(g.fire(t2), QStringLiteral("uuid-1"));
    QVERIFY(g.fire(t2).isEmpty());
}

void TstPushNotif::gateNewerTapReplacesTheOneInFlight()
{
    DeferredNotifGate g;
    const quint64 t1 = g.loggedIn(QStringLiteral("uuid-old"));
    //homeLoaded() again without a drop (it can be reported twice).
    const quint64 t2 = g.loggedIn(QStringLiteral("uuid-new"));
    QVERIFY(g.fire(t1).isEmpty());
    QCOMPARE(g.fire(t2), QStringLiteral("uuid-new"));

    //Without a newer tap the one in flight is kept by the new login.
    const quint64 t3 = g.loggedIn(QStringLiteral("uuid-a"));
    const quint64 t4 = g.loggedIn(QString());
    QVERIFY(g.fire(t3).isEmpty());
    QCOMPARE(g.fire(t4), QStringLiteral("uuid-a"));
}

void TstPushNotif::tapDuringEveryReconnectionIsOpened()
{
    //The FIX-3 regression: before, only the first login of the process
    //consumed a deferred tap, so the one tapped during a later reconnection
    //was silently dropped.
    FakeApp app;
    QVERIFY(app.delayExpired(app.homeLoaded()).isEmpty()); //first login, no tap

    for (int cycle = 1; cycle <= 3; cycle++)
    {
        app.sessionDown();
        const QString uuid = QStringLiteral("uuid-cycle-%1").arg(cycle);
        hw()->handleNotificationOpened(uuid);

        const quint64 ticket = app.homeLoaded();
        QVERIFY2(ticket != 0, qPrintable(uuid));
        QCOMPARE(app.delayExpired(ticket), uuid);
        QVERIFY(app.delayExpired(ticket).isEmpty());
        QVERIFY(!hw()->hasPendingNotif());
    }
}

void TstPushNotif::tapTakenButSessionDroppedOpensAtNextLogin()
{
    FakeApp app;
    hw()->handleNotificationOpened(QStringLiteral("uuid-1"));

    const quint64 t1 = app.homeLoaded();
    //Lost again before the 400 ms delay expired.
    app.sessionDown();
    QVERIFY(hw()->hasPendingNotif());
    QVERIFY(app.delayExpired(t1).isEmpty());

    const quint64 t2 = app.homeLoaded();
    QCOMPARE(app.delayExpired(t2), QStringLiteral("uuid-1"));
    //Opened exactly once over the two logins.
    app.sessionDown();
    QVERIFY(!hw()->hasPendingNotif());
    QVERIFY(app.delayExpired(app.homeLoaded()).isEmpty());
}

//A tap deferred while the session was down (reconnecting), then an explicit
//logout, then a manual login: nothing opens (it could belong to another
//home, and the user closed that session on purpose).
void TstPushNotif::tapPendingThenUserLogoutOpensNothing()
{
    FakeApp app;
    QVERIFY(app.delayExpired(app.homeLoaded()).isEmpty()); //logged in, no tap
    app.sessionDown(); //connection lost
    hw()->handleNotificationOpened(QStringLiteral("uuid-stale"));
    QVERIFY(hw()->hasPendingNotif());

    app.userLogout();
    QVERIFY(!hw()->hasPendingNotif());

    const quint64 ticket = app.homeLoaded(); //next manual login
    QCOMPARE(ticket, quint64(0));
    QVERIFY(app.delayExpired(ticket).isEmpty());
}

//Taken by homeLoaded() but the user logs out before the 400 ms delay: the
//armed delay is stale and the tap is not handed to the next login.
void TstPushNotif::tapInFlightThenUserLogoutOpensNothing()
{
    FakeApp app;
    hw()->handleNotificationOpened(QStringLiteral("uuid-in-flight"));
    const quint64 t1 = app.homeLoaded();
    QVERIFY(t1 != 0);

    app.userLogout();
    QVERIFY(!hw()->hasPendingNotif());
    QVERIFY(app.delayExpired(t1).isEmpty());

    const quint64 t2 = app.homeLoaded();
    QCOMPARE(t2, quint64(0));
    QVERIFY(app.delayExpired(t2).isEmpty());
}

//Unchanged: the login cancel button (plain logout()) is not a user logout,
//the tap is still opened by the next login.
void TstPushNotif::tapPendingThenLoginCancelStillOpens()
{
    FakeApp app;
    hw()->handleNotificationOpened(QStringLiteral("uuid-kept"));
    const quint64 t1 = app.homeLoaded();

    app.sessionDown(); //Application::logout() from the cancel button
    QVERIFY(hw()->hasPendingNotif());
    QVERIFY(app.delayExpired(t1).isEmpty());

    const quint64 t2 = app.homeLoaded();
    QCOMPARE(app.delayExpired(t2), QStringLiteral("uuid-kept"));
}

//Only what was pending at the logout is forgotten: a tap made after it is
//opened by the next login as usual.
void TstPushNotif::tapAfterUserLogoutOpensAtNextLogin()
{
    FakeApp app;
    hw()->handleNotificationOpened(QStringLiteral("uuid-old"));
    app.userLogout();
    hw()->handleNotificationOpened(QStringLiteral("uuid-new"));

    const quint64 ticket = app.homeLoaded();
    QCOMPARE(app.delayExpired(ticket), QStringLiteral("uuid-new"));
}

//iOS Quick Action: Application only runs it when hasStartedWithOption() is
//true at homeLoaded(); after a user logout it must be false.
void TstPushNotif::userLogoutForgetsTheStartOption()
{
    StartOptionHw h;
    h.startWithOption();
    h.handleNotificationOpened(QStringLiteral("uuid-x"));
    QVERIFY(h.hasStartedWithOption());
    QVERIFY(h.hasPendingNotif());

    h.forgetPendingUserActions();
    QVERIFY(!h.hasStartedWithOption());
    QVERIFY(!h.hasPendingNotif());
}

//Settings -> Reset all data: the next login may be on another home, so a
//pending or in-flight tap from the old one must not open.
void TstPushNotif::resetAllDataOpensNothing()
{
    FakeApp app;
    hw()->handleNotificationOpened(QStringLiteral("uuid-old-home-1"));
    const quint64 t1 = app.homeLoaded(); //taken, delay armed
    QVERIFY(t1 != 0);
    app.sessionDown(); //lost before the delay: handed back
    hw()->handleNotificationOpened(QStringLiteral("uuid-old-home-2"));
    QVERIFY(hw()->hasPendingNotif());

    app.resetAllData();
    QVERIFY(!hw()->hasPendingNotif());
    QVERIFY(app.delayExpired(t1).isEmpty());

    const quint64 t2 = app.homeLoaded(); //login on the new home
    QCOMPARE(t2, quint64(0));
    QVERIFY(app.delayExpired(t2).isEmpty());
}

// ---------------------------------------------------------------- push token

void TstPushNotif::tokenChangeIsSignalledOnce()
{
    QSignalSpy changed(hw(), &HardwareUtils::deviceTokenChanged);
    const QString t = freshToken();

    hw()->setDeviceToken(t);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(hw()->getDeviceToken(), t);

    //Android reports the stored token, then FCM's answer: often the same.
    hw()->setDeviceToken(t);
    QCOMPARE(changed.count(), 1);

    //An empty value (no token yet) never erases a known one.
    hw()->setDeviceToken(QString());
    QCOMPARE(changed.count(), 1);
    QCOMPARE(hw()->getDeviceToken(), t);
}

void TstPushNotif::clearedTokenIsSignalled()
{
    hw()->setDeviceToken(freshToken());
    QSignalSpy changed(hw(), &HardwareUtils::deviceTokenChanged);

    hw()->clearDeviceToken();
    QCOMPARE(changed.count(), 1);
    QVERIFY(hw()->getDeviceToken().isEmpty());

    hw()->clearDeviceToken();
    QCOMPARE(changed.count(), 1);
}

void TstPushNotif::describedTokenIsTruncated()
{
    const QString t = freshToken();
    const QString d = HardwareUtils::describeToken(t);

    QVERIFY(!d.contains(t));
    QVERIFY(d.startsWith(t.left(8)));
    QVERIFY(d.contains(QString::number(t.size())));
    QCOMPARE(HardwareUtils::describeToken(QString()), QStringLiteral("<empty>"));
}

// ---------------------------------------------------------------- HTTP transport

void TstPushNotif::httpRegistersAtLogin()
{
    const QString t = freshToken();
    hw()->setDeviceToken(t);

    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    startConnection(http->baseUrl());

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(http->registrations.size(), 1);
    QCOMPARE(http->registrations.at(0).value(QStringLiteral("token")).toString(), t);
    verifyAuthenticated(http->registrations.at(0));

    //Nothing else is sent on its own, and nothing was refused.
    pump(200);
    QCOMPARE(http->registrations.size(), 1);
    QCOMPARE(http->rejected, QStringList());
}

void TstPushNotif::httpRotationWhileConnectedRegistersAgain()
{
    hw()->setDeviceToken(freshToken());

    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    startConnection(http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 1);
    verifyAuthenticated(http->registrations.at(0));

    //FCM onNewToken() while logged in.
    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    QTRY_COMPARE(http->registrations.size(), 2);
    QCOMPARE(http->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);
    verifyAuthenticated(http->registrations.at(1));

    //Same token reported again (stored value + getToken()): no duplicate.
    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 2);
    QCOMPARE(http->logins, 1);
    QCOMPARE(http->rejected, QStringList());
    QVERIFY(!connection->hasPendingTlsTrust());

    //The store dies with this scope.
    connection->logout();
    pump(50);
    delete connection;
    connection = nullptr;
}

void TstPushNotif::httpTokenArrivingAfterLoginIsRegistered()
{
    //iOS cold start: APNs answers after the login completed. No token at all
    //when the session starts (init() cleared it).
    QVERIFY(hw()->getDeviceToken().isEmpty());

    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    startConnection(http->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    pump(200);
    QCOMPARE(http->registrations.size(), 0);

    const QString late = freshToken();
    hw()->setDeviceToken(late);
    QTRY_COMPARE(http->registrations.size(), 1);
    QCOMPARE(http->registrations.at(0).value(QStringLiteral("token")).toString(), late);
    verifyAuthenticated(http->registrations.at(0));
    QCOMPARE(http->logins, 1);
}

void TstPushNotif::httpTokenChangeDuringLoginIsSentOnce()
{
    hw()->setDeviceToken(freshToken());

    http = new FakeHttpServer();
    http->loginDelayMs = 1000;
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    startConnection(http->baseUrl());

    //The token rotates while the login is in flight: nothing can be sent
    //yet, and the login must register the newest token, once.
    pump(100);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Connecting);
    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(http->registrations.size(), 1);
    QCOMPARE(http->registrations.at(0).value(QStringLiteral("token")).toString(), rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 1);
}

void TstPushNotif::httpTokenChangeWhileLoggedOutWaitsForLogin()
{
    hw()->setDeviceToken(freshToken());

    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    startConnection(http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 1);

    connection->logout();
    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 1);

    connection->login(TestUser, TestPass, http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 2);
    QCOMPARE(http->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);
    verifyAuthenticated(http->registrations.at(1));
    pump(200);
    QCOMPARE(http->registrations.size(), 2);
}

void TstPushNotif::httpFallbackFromWebsocketRegistersOnce()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    const QString t = freshToken();
    hw()->setDeviceToken(t);

    //A bare host: the mobile build tries wss://<host>/api first and falls
    //back to https://<host>/api.php. This server refuses the websocket
    //upgrade (400, like a calaos_server without websocket), so the HTTP
    //fallback logs in and must register the token exactly once.
    http = new FakeHttpServer(&tlsConfig);
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));

    //A self-signed certificate is only accepted once pinned: as if the user
    //had confirmed it at an earlier login.
    TlsPinStore pins(certDir.filePath(QStringLiteral("pins.ini")));
    pins.setPin(TlsPinning::hostKey(QStringLiteral("127.0.0.1"), http->serverPort()),
                TlsPinning::spkiSha256(tlsConfig.localCertificate()));
    connection = new CalaosConnection();
    connection->setPinStore(&pins);
    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(http->serverPort()));

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(connection->isHttp());
    QVERIFY(http->getRequests >= 1);
    QTRY_COMPARE(http->registrations.size(), 1);
    QCOMPARE(http->registrations.at(0).value(QStringLiteral("token")).toString(), t);
    verifyAuthenticated(http->registrations.at(0));

    pump(300);
    QCOMPARE(http->registrations.size(), 1);
    QCOMPARE(http->logins, 1);
    QCOMPARE(http->rejected, QStringList());
}

// ---------------------------------------------------------------- websocket transport

void TstPushNotif::wsRegistersAfterTheLoginIsAccepted()
{
    const QString t = freshToken();
    hw()->setDeviceToken(t);

    ws = new FakeWsServer();
    //Slower than the 100 ms the old code waited before sending register_push
    //blindly: that frame was dropped, the socket not being logged in yet.
    ws->loginDelayMs = 400;
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    startConnection(ws->url());

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(ws->registrations.size(), 1);
    QCOMPARE(ws->registrations.at(0).value(QStringLiteral("token")).toString(), t);
    QVERIFY(!ws->registrations.at(0).contains(QStringLiteral("push_format")));
    verifyDeviceName(ws->registrations.at(0));
    //The websocket session is already authenticated: no credentials repeated.
    QVERIFY(!ws->registrations.at(0).contains(QStringLiteral("cn_user")));
    QVERIFY(!ws->registrations.at(0).contains(QStringLiteral("cn_pass")));
    QVERIFY(ws->received.indexOf(QStringLiteral("login")) <
            ws->received.indexOf(QStringLiteral("register_push")));

    pump(200);
    QCOMPARE(ws->registrations.size(), 1);
}

void TstPushNotif::wsRotationWhileConnectedRegistersAgain()
{
    hw()->setDeviceToken(freshToken());

    ws = new FakeWsServer();
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    startConnection(ws->url());
    QTRY_COMPARE(ws->registrations.size(), 1);

    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    QTRY_COMPARE(ws->registrations.size(), 2);
    QCOMPARE(ws->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);

    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(ws->registrations.size(), 2);
}

// ---------------------------------------------------------------- push format

//The Android build of registerPushToken() (hardware "android") asks
//calaos_server for data-only FCM messages: the one field of the contract, in
//the one place that builds the payload.
void TstPushNotif::payloadAsksDataFormatForAndroid()
{
    const QJsonObject o = PushFormat::registration(QStringLiteral("tok"), QStringLiteral("android"));
    QCOMPARE(o.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
    QCOMPARE(o.value(QStringLiteral("hardware")).toString(), QStringLiteral("android"));
    QCOMPARE(o.value(QStringLiteral("push_format")).toString(), QStringLiteral("data"));
    QCOMPARE(o.size(), 3);
}

//iOS keeps its APNs path, and an unknown hardware (desktop, these tests)
//sends the legacy payload untouched.
void TstPushNotif::payloadHasNoFormatForOtherHardware()
{
    for (const QString &hw: { QStringLiteral("ios"), QString() })
    {
        const QJsonObject o = PushFormat::registration(QStringLiteral("tok"), hw);
        QCOMPARE(o.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
        QCOMPARE(o.value(QStringLiteral("hardware")).toString(), hw);
        QVERIFY(!o.contains(QStringLiteral("push_format")));
        QCOMPARE(o.size(), 2);
    }
}

//The admin picks the approver phone by this name: sent as is, trimmed.
void TstPushNotif::payloadCarriesTheDeviceName()
{
    const QJsonObject a = PushFormat::registration(QStringLiteral("tok"), QStringLiteral("android"),
                                                   QStringLiteral("  Pixel de Raoul\n"));
    QCOMPARE(a.value(QStringLiteral("device_name")).toString(), QStringLiteral("Pixel de Raoul"));
    QCOMPARE(a.value(QStringLiteral("push_format")).toString(), QStringLiteral("data"));
    QCOMPARE(a.size(), 4);

    const QJsonObject i = PushFormat::registration(QStringLiteral("tok"), QStringLiteral("ios"),
                                                   QStringLiteral("iPhone"));
    QCOMPARE(i.value(QStringLiteral("device_name")).toString(), QStringLiteral("iPhone"));
    QCOMPARE(i.value(QStringLiteral("token")).toString(), QStringLiteral("tok"));
    QCOMPARE(i.value(QStringLiteral("hardware")).toString(), QStringLiteral("ios"));
    QCOMPARE(i.size(), 3);
}

//No usable name: the field is left out rather than sent empty.
void TstPushNotif::payloadWithoutDeviceNameOmitsTheField()
{
    for (const QString &name: { QString(), QStringLiteral(""), QStringLiteral(" \t ") })
    {
        const QJsonObject o = PushFormat::registration(QStringLiteral("tok"), QStringLiteral("ios"), name);
        QVERIFY(!o.contains(QStringLiteral("device_name")));
        QCOMPARE(o.size(), 2);
    }
}

// ---------------------------------------------------------------- logs

void TstPushNotif::cleanupTestCase()
{
    qInstallMessageHandler(s_previousHandler);

    //Whatever subset of the functions ran: no token and no password in clear.
    for (const QString &line: std::as_const(s_logLines))
    {
        for (const QString &t: std::as_const(usedTokens()))
            QVERIFY2(!line.contains(t), qPrintable(QStringLiteral("token in clear in a log line of %1 chars")
                                                   .arg(line.size())));
        QVERIFY2(!line.contains(TestPass), "password in clear in a log line");
    }

#ifdef QT_DEBUG
    //When the HTTP fallback really sent register_push, its debug dump is
    //there, redacted.
    if (s_httpRegistrations > 0)
    {
        bool sawRedactedRegister = false;
        for (const QString &line: std::as_const(s_logLines))
        {
            if (line.startsWith(QLatin1String("SEND:")) &&
                line.contains(QLatin1String("register_push")) &&
                line.contains(QLatin1String("\"token\": \"***\"")) &&
                line.contains(QLatin1String("\"cn_pass\": \"***\"")))
                sawRedactedRegister = true;
        }
        QVERIFY(sawRedactedRegister);
    }
#endif
}

QTEST_MAIN(TstPushNotif)
#include "tst_pushnotif.moc"
