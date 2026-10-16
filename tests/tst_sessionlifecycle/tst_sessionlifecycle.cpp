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
 * FIX-8: the network coming back used to reload the stored credentials before
 * resume(), replacing a login typed by hand and not accepted yet (observed: a
 * wrong password replaced by the stored demo account, and the app went into
 * demo mode). resume() now takes no credentials and reconnects with those of
 * the session it resumes; the server records which credentials each login
 * carried:
 *
 *  - typed but rejected credentials + network change: no login with any other
 *    credentials, never connected;
 *  - active session + network change: reconnects with the session's
 *    credentials;
 *  - acceptsImmediateReconnect() (Application only reads the stored settings
 *    for discovery when it says yes) follows the policy.
 *
 * FIX-9: launched without network, the launch login was refused by
 * Application::login()'s network check before anything recorded that a
 * session was wanted, so the link coming back resumed nothing. Application
 * now hands a login refused offline to deferLogin():
 *
 *  - offline launch, then the network: exactly one login;
 *  - logout, then an automatic login offline, then the network: none;
 *  - offline launch with the stored credentials, the user types others and
 *    taps Login still offline, then the network: the typed ones are used.
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
//What the user types by mistake on the login screen.
static const QString TypedUser = QStringLiteral("typed-user");
static const QString TypedPass = QStringLiteral("wrong-pass");

/*
 * calaos_server's api.php reduced to what a session needs: get_home (the
 * login, counted), poll_listen register then a long-poll held open, and a
 * 200 for anything else.
 */
class FakeApiServer: public QTcpServer
{
    Q_OBJECT

public:
    //Successful logins.
    int logins = 0;
    //Every login attempt, accepted or not: "user:pass".
    QStringList attempts;
    //Only these credentials are accepted.
    QString acceptedUser = TestUser;
    QString acceptedPass = TestPass;

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
            const QString user = j.value(QStringLiteral("cn_user")).toString();
            const QString pass = j.value(QStringLiteral("cn_pass")).toString();
            attempts.append(user + QLatin1Char(':') + pass);
            if (user != acceptedUser || pass != acceptedPass)
            {
                //What the HTTP API answers to wrong credentials.
                reply(sock, "{\"success\":\"false\"}");
                return;
            }
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

    void typedCredentialsRejectedThenNetworkChangeUsesNoOtherCredentials();
    void activeSessionResumesWithTheSessionCredentials();
    void acceptsImmediateReconnectFollowsThePolicy();

    void offlineLaunchConnectsWhenTheNetworkIsBack();
    void logoutThenOfflineLaunchStaysLoggedOut();
    void offlineLaunchThenTypedLoginUsesTheTypedCredentials();

private:
    //The session goes up through an explicit login, as from the login screen.
    void loginAndWait();
    //What Application does on a network change (networkStatusChanged()).
    void networkDown() { connection->suspend(); }
    //No credentials: Application no longer reloads the stored ones (FIX-8).
    void networkUp() { connection->resume(); }
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
    //What Application::startLogin() does when the network check fails
    //(FIX-9): the launch login and iOS become-active are automatic, the
    //login screen is a user action.
    bool launchLoginOffline(const QString &user, const QString &pass)
    {
        return connection->deferLogin(user, pass, server->baseUrl(), ReconnectPolicy::AutoRetry);
    }
    bool typedLoginOffline(const QString &user, const QString &pass)
    {
        return connection->deferLogin(user, pass, server->baseUrl(), ReconnectPolicy::UserRequest);
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

//FIX-8, the ticket's case: the stored (accepted) credentials are user/pass,
//the user types a wrong password, the server rejects it, then the network
//goes down and comes back while the retry is pending. Before the fix
//Application reloaded the stored credentials first and resumed with them.
void TstSessionLifecycle::typedCredentialsRejectedThenNetworkChangeUsesNoOtherCredentials()
{
    const QString typed = TypedUser + QLatin1Char(':') + TypedPass;

    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(server->attempts.size(), 1);
    //Rejected once: a retry is pending, the session is still wanted, which
    //is exactly when resume() acts.
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Reconnecting);
    QVERIFY(connection->failingOnCredentials());

    networkDown();
    networkUp();
    //The resume really happened (otherwise this test proves nothing) and
    //used the typed credentials.
    QTRY_COMPARE(server->attempts.size(), 2);

    //Let the policy run to its give-up on the credentials.
    for (int i = 0; i < 4; i++)
        settle();

    QCOMPARE(server->logins, 0);
    QVERIFY(server->attempts.size() >= 2);
    for (const QString &a: std::as_const(server->attempts))
        QCOMPARE(a, typed);
    QVERIFY(connection->connectionState() != ReconnectPolicy::Connected);

    //And after the give-up, the network changing again starts nothing.
    const int count = server->attempts.size();
    networkDown();
    networkUp();
    settle();
    QCOMPARE(server->attempts.size(), count);
}

//FIX-8: an active session that loses the link reconnects with its own
//credentials and host, whatever the caller may hold at that moment.
void TstSessionLifecycle::activeSessionResumesWithTheSessionCredentials()
{
    loginAndWait();
    const QString session = TestUser + QLatin1Char(':') + TestPass;

    networkDown();
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);

    QCOMPARE(server->logins, 2);
    QCOMPARE(server->attempts, QStringList({ session, session }));
}

//Application::calaosServerDetected() only reads the stored settings when
//this says an attempt would start.
void TstSessionLifecycle::acceptsImmediateReconnectFollowsThePolicy()
{
    //Nothing tried yet: discovery is how the desktop logs in the first time.
    QVERIFY(connection->acceptsImmediateReconnect());

    loginAndWait();
    QVERIFY(!connection->acceptsImmediateReconnect()); //session up

    connection->logout();
    settle(); //past the anti-chatter delay
    QVERIFY(!connection->acceptsImmediateReconnect()); //closed on purpose

    loginAndWait();
    networkDown();
    settle();
    QVERIFY(connection->acceptsImmediateReconnect()); //wanted, link down

    connection->requestImmediateReconnect(TestUser, TestPass, server->baseUrl());
    QVERIFY(!connection->acceptsImmediateReconnect()); //attempt in flight
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
}

//FIX-9, the ticket's case: the app is launched in airplane mode, airplane
//mode is turned off. Android reports the link going down at launch too, then
//flapping before it settles: still exactly one login.
void TstSessionLifecycle::offlineLaunchConnectsWhenTheNetworkIsBack()
{
    networkDown();
    QVERIFY(launchLoginOffline(TestUser, TestPass));
    settle();
    QCOMPARE(server->attempts.size(), 0); //nothing while the link is down
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QVERIFY(!connection->sessionClosedByUser());

    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    networkUp(); //a second "link is back" while connected costs nothing
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(server->attempts, QStringList({ TestUser + QLatin1Char(':') + TestPass }));

    //From then on it is an ordinary session.
    networkDown();
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);
}

//FIX-9: an automatic login refused offline (iOS become-active, the launch
//login) never reopens a session the user closed.
void TstSessionLifecycle::logoutThenOfflineLaunchStaysLoggedOut()
{
    loginAndWait();
    connection->logout();

    networkDown();
    QVERIFY(!launchLoginOffline(TestUser, TestPass));
    QVERIFY(connection->sessionClosedByUser());
    networkUp();
    networkDown();
    networkUp();
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    //Tapping Login still works, offline too.
    networkDown();
    QVERIFY(typedLoginOffline(TestUser, TestPass));
    QVERIFY(!connection->sessionClosedByUser());
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);
}

//FIX-9 + FIX-8: launched offline with the stored credentials, the user types
//other ones and taps Login while still offline. The network coming back
//logs in with the latest explicit intent, the typed credentials, and nothing
//else.
void TstSessionLifecycle::offlineLaunchThenTypedLoginUsesTheTypedCredentials()
{
    //Here the typed account is the valid one (a new home, or a fixed password).
    server->acceptedUser = TypedUser;
    server->acceptedPass = TypedPass;
    const QString typed = TypedUser + QLatin1Char(':') + TypedPass;

    networkDown();
    QVERIFY(launchLoginOffline(TestUser, TestPass));
    QVERIFY(typedLoginOffline(TypedUser, TypedPass));
    settle();
    QCOMPARE(server->attempts.size(), 0);

    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    settle();
    QCOMPARE(server->logins, 1);
    QCOMPARE(server->attempts, QStringList({ typed }));
}

QTEST_MAIN(TstSessionLifecycle)
#include "tst_sessionlifecycle.moc"
