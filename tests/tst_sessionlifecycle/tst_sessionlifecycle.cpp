/*
 * Session lifecycle of CalaosConnection (FIX-7).
 *
 * Reported on Android: after Settings -> Logout, a network change (airplane
 * mode on/off, Wi-Fi <-> 4G) seemed to log the app back in. Application turns
 * those changes into suspend()/resume(), iOS turns going to the background
 * and back into pause()/unpause() followed by a login unless the user logged
 * out. These cases drive a real CalaosConnection exactly like Application
 * does and count the logins the server actually receives:
 *
 *  - logout, then the network flaps: no login;
 *  - logout, then server discovery (desktop): no login;
 *  - an active session that loses the network logs in again when it is back;
 *  - logging in by hand after a logout works;
 *  - iOS: the network flapping in the background does not log in, the app
 *    coming back does when a session was up, does not after a logout.
 *
 * HTTP transport only: the policy is shared by both transports and the HTTP
 * fake is enough to count logins (one get_home per login).
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "CalaosConnection.h"

static const QString TestUser = QStringLiteral("user");
static const QString TestPass = QStringLiteral("pass");

/*
 * calaos_server's api.php reduced to what a session needs: get_home (the
 * login, counted), poll_listen register then a long-poll held open, and a
 * 200 for anything else.
 */
class FakeApiServer: public QTcpServer
{
    Q_OBJECT

public:
    int logins = 0;

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
            reply(sock, "{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}");
            return;
        }

        if (action == QLatin1String("poll_listen"))
        {
            if (j.value(QStringLiteral("type")).toString() == QLatin1String("register"))
                reply(sock, "{\"success\":\"true\",\"uuid\":\"fake-poll-uuid\"}");
            //"get": held open, like a long-poll with no event.
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

class TstSessionLifecycle: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void logoutThenNetworkFlapStaysLoggedOut();
    void logoutThenDiscoveryStaysLoggedOut();
    void activeSessionReconnectsWhenTheNetworkIsBack();
    void loginAfterLogoutWorks();

    void iosBackgroundNetworkFlapDoesNotLogIn();
    void iosForegroundLogsBackIn();
    void iosLogoutThenBackgroundAndForegroundStaysLoggedOut();

private:
    //The session goes up through an explicit login, as from the login screen.
    void loginAndWait();
    //What Application does on a network change (networkStatusChanged()).
    void networkDown() { connection->suspend(); }
    void networkUp() { connection->resume(TestUser, TestPass, server->baseUrl()); }
    //What Application does when iOS reports the app going to the background
    //and coming back (createQmlApp() lambdas): returns true when Application
    //would call login() again.
    void appBackground() { connection->pause(); }
    bool appForeground()
    {
        connection->unpause();
        if (connection->sessionClosedByUser())
            return false;
        connection->login(TestUser, TestPass, server->baseUrl());
        return true;
    }
    //Longer than the anti-chatter delay of requestImmediateReconnect() and
    //than the first backoff delay: anything that was going to start a login
    //has done it by then.
    static void settle() { QTest::qWait(ReconnectPolicy::BaseDelayMs + 500); }

    FakeApiServer *server = nullptr;
    CalaosConnection *connection = nullptr;
};

void TstSessionLifecycle::init()
{
    server = new FakeApiServer();
    QVERIFY(server->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
}

void TstSessionLifecycle::cleanup()
{
    delete connection;
    connection = nullptr;
    delete server;
    server = nullptr;
}

void TstSessionLifecycle::loginAndWait()
{
    const int before = server->logins;
    connection->login(TestUser, TestPass, server->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, before + 1);
}

//The ticket's case: Logout, then airplane mode on and off (three times).
void TstSessionLifecycle::logoutThenNetworkFlapStaysLoggedOut()
{
    loginAndWait();
    connection->logout();
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    for (int i = 0; i < 3; i++)
    {
        networkDown();
        networkUp();
    }
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QVERIFY(connection->sessionClosedByUser());
}

//Desktop: a calaos_server announce after a logout does not log in either.
void TstSessionLifecycle::logoutThenDiscoveryStaysLoggedOut()
{
    loginAndWait();
    connection->logout();
    settle(); //past the anti-chatter delay, so only the policy can refuse

    connection->requestImmediateReconnect(TestUser, TestPass, server->baseUrl());
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
}

//Kept behaviour: a session that was up when the link went down comes back.
void TstSessionLifecycle::activeSessionReconnectsWhenTheNetworkIsBack()
{
    loginAndWait();

    for (int i = 0; i < 2; i++)
    {
        networkDown();
        QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
        settle();
        QCOMPARE(server->logins, i + 1); //nothing while the link is down

        networkUp();
        QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
        QCOMPARE(server->logins, i + 2);
    }

    //A second "link is back" while connected costs nothing.
    networkUp();
    settle();
    QCOMPARE(server->logins, 3);
}

void TstSessionLifecycle::loginAfterLogoutWorks()
{
    loginAndWait();
    connection->logout();
    networkDown();
    networkUp();

    loginAndWait();
    QCOMPARE(server->logins, 2);
    QVERIFY(!connection->sessionClosedByUser());

    //The new session follows the normal rules again.
    networkDown();
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 3);
}

//iOS: a Reachability change seen while in the background does not log in.
void TstSessionLifecycle::iosBackgroundNetworkFlapDoesNotLogIn()
{
    loginAndWait();
    appBackground();
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    networkDown();
    networkUp();
    connection->requestImmediateReconnect(TestUser, TestPass, server->baseUrl());
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
}

//iOS: back to the foreground with a session that was up -> logged in again,
//and the network changes after that behave as usual.
void TstSessionLifecycle::iosForegroundLogsBackIn()
{
    loginAndWait();
    appBackground();
    networkDown();
    networkUp();

    QVERIFY(appForeground());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);

    networkDown();
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 3);
}

//iOS: Logout, switch to another app, come back: still logged out, and the
//login screen still logs in.
void TstSessionLifecycle::iosLogoutThenBackgroundAndForegroundStaysLoggedOut()
{
    loginAndWait();
    connection->logout();

    appBackground();
    networkDown();
    networkUp();
    QVERIFY(!appForeground());
    networkDown();
    networkUp();
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    loginAndWait();
    QCOMPARE(server->logins, 2);
}

QTEST_MAIN(TstSessionLifecycle)
#include "tst_sessionlifecycle.moc"
