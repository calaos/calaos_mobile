/*
 * Push notification plumbing shared by iOS and Android (FIX-3).
 *
 * Bugs covered:
 *  1. A notification tapped while the session was down (reconnection) was
 *     lost: the deferred tap was only consumed on the very first login of the
 *     process. HardwareUtils now keeps it until Application takes it, once,
 *     at the next homeLoaded() (takePendingNotif()). The Application side is
 *     a single call to takePendingNotif() and needs the whole QML app, it is
 *     checked on device; the HardwareUtils rules are checked here.
 *  2. A rotated push token only reached calaos_server at the next login.
 *     HardwareUtils emits deviceTokenChanged() and CalaosConnection sends
 *     register_push again while the session is up, never twice for the same
 *     token in one session (PushRegistration).
 *  3. The push token was dumped in clear by the debug log of the HTTP
 *     fallback (sendHttp()): "token" is now a redacted key.
 *
 * The CalaosConnection cases run a real connection against in-process fake
 * servers (HTTP api.php and websocket /api): no calaos_server, no network
 * outside the loopback.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include "CalaosConnection.h"
#include "HardwareUtils.h"

//Every log line of the process, to check that no push token leaks in clear.
static QStringList s_logLines;
static QtMessageHandler s_previousHandler = nullptr;

static void captureMessages(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    s_logLines << msg;
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}

/*
 * Plays calaos_server's api.php: answers the get_home login, records every
 * register_push, hands out a polling uuid and then holds the long-poll open
 * (a quiet installation), so the session stays up without any traffic.
 */
class FakeHttpServer: public QTcpServer
{
    Q_OBJECT

public:
    using QTcpServer::QTcpServer;

    //Delay before the login (get_home) is answered.
    int loginDelayMs = 0;
    int logins = 0;
    QList<QJsonObject> registrations;

    QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1/api.php").arg(serverPort());
    }

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        QByteArray *buf = new QByteArray();
        connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf]()
        {
            buf->append(sock->readAll());
            const int hdrEnd = buf->indexOf("\r\n\r\n");
            if (hdrEnd < 0)
                return;
            int contentLength = 0;
            for (const QByteArray &line: buf->left(hdrEnd).split('\n'))
            {
                if (line.toLower().startsWith("content-length:"))
                    contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
            const QByteArray body = buf->mid(hdrEnd + 4);
            if (body.size() < contentLength)
                return;
            buf->clear();
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

        if (action == QLatin1String("get_home"))
        {
            logins++;
            QTimer::singleShot(loginDelayMs, sock, [sock]()
            {
                reply(sock, "{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}");
            });
            return;
        }

        if (action == QLatin1String("register_push"))
        {
            registrations << j;
            reply(sock, "{\"success\":\"true\"}");
            return;
        }

        if (action == QLatin1String("poll_listen"))
        {
            if (j.value(QStringLiteral("type")).toString() == QLatin1String("register"))
                reply(sock, "{\"success\":\"true\",\"uuid\":\"fake-poll-uuid\"}");
            //"get": never answered, like a long-poll with no event.
            return;
        }

        reply(sock, "{\"success\":\"true\"}");
    }

    static void reply(QTcpSocket *sock, const QByteArray &body)
    {
        if (sock->state() != QAbstractSocket::ConnectedState)
            return;
        QByteArray out = "HTTP/1.1 200 OK\r\nContent-Type: application/json";
        out += "\r\nContent-Length: " + QByteArray::number(body.size());
        out += "\r\nConnection: close\r\n\r\n";
        out += body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }
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

    //HardwareUtils: push token.
    void tokenChangeIsSignalledOnce();
    void describedTokenIsTruncated();

    //CalaosConnection over HTTP (the transport whose debug log leaked).
    void httpRegistersAtLogin();
    void httpRotationWhileConnectedRegistersAgain();
    void httpTokenArrivingAfterLoginIsRegistered();
    void httpTokenChangeDuringLoginIsSentOnce();
    void httpTokenChangeWhileLoggedOutWaitsForLogin();

    //CalaosConnection over websocket.
    void wsRegistersAfterTheLoginIsAccepted();
    void wsRotationWhileConnectedRegistersAgain();

    //No token in clear in the logs, whatever ran above.
    void cleanupTestCase();

private:
    static HardwareUtils *hw() { return HardwareUtils::Instance(); }
    static void pump(int ms);
    //A token never used before in this run (HardwareUtils is a singleton
    //and ignores a token equal to the current one).
    static QString freshToken();
    //Every token handed to HardwareUtils, to scan the logs at the end.
    static QStringList &usedTokens();

    FakeHttpServer *http = nullptr;
    FakeWsServer *ws = nullptr;
    CalaosConnection *connection = nullptr;
};

void TstPushNotif::initTestCase()
{
    s_previousHandler = qInstallMessageHandler(captureMessages);
}

void TstPushNotif::init()
{
    hw()->updateCalaosConnectState(false);
    hw()->takePendingNotif();
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

    //And the same holds for a second disconnection/reconnection cycle: this
    //is the case that used to be dropped after the first login.
    hw()->updateCalaosConnectState(true);
    hw()->updateCalaosConnectState(false);
    hw()->handleNotificationOpened(QStringLiteral("uuid-2"));
    QCOMPARE(hw()->takePendingNotif(), QStringLiteral("uuid-2"));
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
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(http->registrations.size(), 1);
    QCOMPARE(http->registrations.at(0).value(QStringLiteral("token")).toString(), t);

    //Nothing else is sent on its own.
    pump(200);
    QCOMPARE(http->registrations.size(), 1);
}

void TstPushNotif::httpRotationWhileConnectedRegistersAgain()
{
    hw()->setDeviceToken(freshToken());

    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 1);

    //FCM onNewToken() while logged in.
    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    QTRY_COMPARE(http->registrations.size(), 2);
    QCOMPARE(http->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);

    //Same token reported again (stored value + getToken()): no duplicate.
    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 2);
    QCOMPARE(http->logins, 1);
}

void TstPushNotif::httpTokenArrivingAfterLoginIsRegistered()
{
    //iOS cold start: APNs answers after the login completed. Simulated with
    //a token change right after login, the previous token being already
    //registered by an earlier session of the process.
    http = new FakeHttpServer();
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(http->registrations.size(), 1);

    const QString late = freshToken();
    hw()->setDeviceToken(late);
    QTRY_COMPARE(http->registrations.size(), 2);
    QCOMPARE(http->registrations.last().value(QStringLiteral("token")).toString(), late);
}

void TstPushNotif::httpTokenChangeDuringLoginIsSentOnce()
{
    hw()->setDeviceToken(freshToken());

    http = new FakeHttpServer();
    http->loginDelayMs = 1000;
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());

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
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 1);

    connection->logout();
    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 1);

    connection->login(QStringLiteral("user"), QStringLiteral("pass"), http->baseUrl());
    QTRY_COMPARE(http->registrations.size(), 2);
    QCOMPARE(http->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);
    pump(200);
    QCOMPARE(http->registrations.size(), 2);
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
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), ws->url());

    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QTRY_COMPARE(ws->registrations.size(), 1);
    QCOMPARE(ws->registrations.at(0).value(QStringLiteral("token")).toString(), t);
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
    connection = new CalaosConnection();
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), ws->url());
    QTRY_COMPARE(ws->registrations.size(), 1);

    const QString rotated = freshToken();
    hw()->setDeviceToken(rotated);
    QTRY_COMPARE(ws->registrations.size(), 2);
    QCOMPARE(ws->registrations.at(1).value(QStringLiteral("token")).toString(), rotated);

    hw()->setDeviceToken(rotated);
    pump(200);
    QCOMPARE(ws->registrations.size(), 2);
}

// ---------------------------------------------------------------- logs

void TstPushNotif::cleanupTestCase()
{
    qInstallMessageHandler(s_previousHandler);

    QVERIFY(!usedTokens().isEmpty());
    for (const QString &line: std::as_const(s_logLines))
    {
        for (const QString &t: std::as_const(usedTokens()))
            QVERIFY2(!line.contains(t), qPrintable(QStringLiteral("token in clear in: ")
                                                   + line.left(120)));
    }

#ifdef QT_DEBUG
    //The HTTP fallback really dumped register_push, redacted.
    bool sawRedactedRegister = false;
    for (const QString &line: std::as_const(s_logLines))
    {
        if (line.startsWith(QLatin1String("SEND:")) &&
            line.contains(QLatin1String("register_push")) &&
            line.contains(QLatin1String("\"token\": \"***\"")))
            sawRedactedRegister = true;
    }
    QVERIFY(sawRedactedRegister);
#endif
}

QTEST_MAIN(TstPushNotif)
#include "tst_pushnotif.moc"
