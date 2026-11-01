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
 * FIX-11: a logout did not survive a restart: the new connection knew
 * nothing of it and the launch login reconnected. The logout is now persisted
 * (HardwareUtils::setSessionActive(), QSettings here) and LaunchLogin applies
 * it to the connection created at startup. A "restart" below is a fresh
 * CalaosConnection set up from what the store holds, exactly like
 * Application::createQmlApp():
 *
 *  - logout, restart: no launch login, no become-active login, nothing when
 *    the network comes back, online or offline;
 *  - then a login by hand succeeds: the state is cleared and the next restart
 *    logs in on its own;
 *  - no stored state (fresh install, update from an older version): the
 *    launch login runs;
 *  - reset all data: the logout is forgotten, the demo account still does
 *    not log in by itself.
 *
 * FIX-10: the modal "No network" alert is replaced by a banner shown while
 * waitsForNetwork() (Application::waitingForNetwork): no network and a
 * session wanted, i.e. one that the link coming back opens. Checked along the
 * same scenarios, next to what the link coming back really does:
 *
 *  - offline launch: shown, hidden once the link is back and connected;
 *  - active session that loses the link: shown during the outage only;
 *  - logout, then offline (automatic login refused): never shown; a Login
 *    tapped offline shows it;
 *  - credentials given up on: not shown, nothing would reconnect.
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
#include <QTemporaryDir>

#include "CalaosConnection.h"
#include "HardwareUtils.h"
#include "LaunchLogin.h"

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
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void logoutThenNetworkFlapStaysLoggedOut();
    void logoutThenDiscoveryStaysLoggedOut();
    void activeSessionReconnectsWhenTheNetworkIsBack();
    void loginAfterLogoutWorks();

    void iosBackgroundNetworkFlapDoesNotLogIn();
    void iosForegroundLogsBackIn();
    void iosLogoutThenBackgroundAndForegroundStaysLoggedOut();
    void iosReturnOverEditedLoginFieldsDoesNotLogIn();
    void wrongPasswordFailsOnceAtOnce();
    void refusalAfterASessionStillRetries();
    void offlineLaunchRefusalStillRetries();
    void iosReturnAfterARefusalDoesNotReplay();
    void iosReturnAfterARefusedStoredPasswordDoesNotReplay();
    void iosReturnAfterAGiveUpDoesNotReplay();
    void iosReturnWorksAgainAfterASuccessfulLogin();
    void iosReturnDropsTheKeptPageWhenItDoesNotLogIn();

    void typedCredentialsRejectedThenNetworkChangeUsesNoOtherCredentials();
    void activeSessionResumesWithTheSessionCredentials();
    void acceptsImmediateReconnectFollowsThePolicy();

    void offlineLaunchConnectsWhenTheNetworkIsBack();
    void logoutThenOfflineAutoLoginStaysLoggedOut();
    void offlineLaunchThenTypedLoginUsesTheTypedCredentials();

    void logoutSurvivesRestart();
    void logoutSurvivesOfflineRestart();
    void loginAfterRestartClearsTheLogout();
    void noStoredStateLaunchLogsIn();
    void resetAllDataForgetsTheLogout();

    void offlineBannerFollowsTheSessionIntent();
    void offlineBannerNotShownAfterLogout();
    void offlineBannerNotShownAfterCredentialsGiveUp();

private:
    //The session goes up through an explicit login, as from the login screen.
    void loginAndWait();
    //What Application does on a network change (networkStatusChanged()).
    void networkDown() { connection->suspend(); }
    //No credentials: Application no longer reloads the stored ones (FIX-8).
    void networkUp() { connection->resume(); }
    //What Application does when iOS reports the app going to the background
    //and coming back (createQmlApp() lambdas): returns true when Application
    //would call login() again. loginEdited is LoginView's flag.
    void appBackground(bool loginEdited = false, bool keepPage = false)
    {
        resumePolicy.resigned(keepPage, loginEdited);
        connection->pause();
    }
    bool appForeground()
    {
        connection->unpause();
        if (!LaunchLogin::shouldLoginOnReturn(connection, resumePolicy))
            return false;
        connection->login(TestUser, TestPass, server->baseUrl());
        return true;
    }
    //Two trips to the password manager: true when neither reached the server.
    bool returnsReachNoServer(bool loginEdited)
    {
        const int before = server->attempts.size();
        for (int i = 0; i < 2; i++)
        {
            appBackground(loginEdited);
            if (appForeground())
                return false;
            settle();
        }
        return server->attempts.size() == before;
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
    //What Application does around the persisted logout (FIX-11):
    //homeLoaded() after any successful login, userLogout() for Settings ->
    //Logout, and createQmlApp() at startup (the app is killed and launched
    //again: a new connection set up from the store only).
    void sessionUp() { HardwareUtils::Instance()->setSessionActive(true); }
    void userLogout()
    {
        HardwareUtils::Instance()->setSessionActive(false);
        connection->logout();
    }
    void restart()
    {
        delete connection;
        connection = new CalaosConnection();
        LaunchLogin::restoreUserLogout(connection, HardwareUtils::Instance());
    }
    //The launch timer of createQmlApp(), online: returns true when it logs in.
    bool launchLogin(const QString &user = TestUser, const QString &pass = TestPass,
                     QString host = QString())
    {
        if (host.isNull())
            host = server->baseUrl();
        if (!LaunchLogin::shouldAutoLogin(connection, user, pass, host))
            return false;
        connection->login(user, pass, host);
        return true;
    }
    //What a fresh QSettings reads, i.e. the next process.
    static QVariant storedSessionState()
    {
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        return settings.value(QStringLiteral("calaos/session_active"));
    }
    static void clearStore()
    {
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.clear();
        settings.sync();
    }

    //Longer than the anti-chatter delay of requestImmediateReconnect() and
    //than the first backoff delay: anything that was going to start a login
    //has done it by then.
    static void settle() { QTest::qWait(ReconnectPolicy::BaseDelayMs + 500); }

    QTemporaryDir *settingsDir = nullptr;
    FakeApiServer *server = nullptr;
    CalaosConnection *connection = nullptr;
    ResumePolicy resumePolicy;
};

void TstSessionLifecycle::initTestCase()
{
    //The base HardwareUtils keeps the session state in QSettings: a store of
    //our own, emptied before every case.
    QCoreApplication::setOrganizationName(QStringLiteral("CalaosTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_sessionlifecycle"));

    //Per-run settings directory: never touch (or clear) the user's real store.
    settingsDir = new QTemporaryDir();
    QVERIFY(settingsDir->isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir->path());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir->path());
}

void TstSessionLifecycle::cleanupTestCase()
{
    clearStore();
    delete settingsDir;
    settingsDir = nullptr;
}

void TstSessionLifecycle::init()
{
    clearStore();
    server = new FakeApiServer();
    QVERIFY(server->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
    resumePolicy.reset();
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

//iOS: no session yet, the user is typing and leaves to copy the password: he
//logs in himself, however many trips it takes.
void TstSessionLifecycle::iosReturnOverEditedLoginFieldsDoesNotLogIn()
{
    QVERIFY(returnsReachNoServer(true));
    QCOMPARE(server->attempts.size(), 0);

    loginAndWait();
    appBackground();
    QVERIFY(appForeground());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);
}

//iOS: the first refusal is enough, the user leaves while the retry is pending
//to fetch the right password. loginEdited stays false: the refusal alone must
//stop the replay.
void TstSessionLifecycle::iosReturnAfterARefusalDoesNotReplay()
{
    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(server->attempts.size(), 1);

    QVERIFY(returnsReachNoServer(false));
    QCOMPARE(server->attempts.size(), 1);
}

//iOS: the stored password stopped working, nothing was ever typed. Only the
//launch login sent it, and no return sends it again.
void TstSessionLifecycle::iosReturnAfterARefusedStoredPasswordDoesNotReplay()
{
    QVERIFY(launchLogin(TypedUser, TypedPass));
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(server->attempts.size(), 1);

    QVERIFY(returnsReachNoServer(false));
}

//A wrong password on the login screen: the server said no, so no second
//attempt. It used to retry with a backoff, the status dropping out of Loading
//between attempts, so the spinner stopped, the alert came seconds later, and a
//tap on the button that looked idle started the whole run again.
void TstSessionLifecycle::wrongPasswordFailsOnceAtOnce()
{
    QSignalSpy failed(connection, &CalaosConnection::loginFailed);
    QSignalSpy states(connection, &CalaosConnection::connectionStateChanged);

    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 8000);

    //The first refusal is the one reported: no attempt came before the alert.
    QCOMPARE(server->attempts.size(), 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    for (const QList<QVariant> &state: std::as_const(states))
        QVERIFY(state.at(0).toInt() != ReconnectPolicy::Reconnecting);

    QTest::qWait(ReconnectPolicy::BaseDelayMs * 3);
    QCOMPARE(server->attempts.size(), 1);
    QCOMPARE(failed.size(), 1);
}

//An attempt that does not come from a UserRequest keeps its bounded retries:
//a session that was up and whose reconnection is refused...
void TstSessionLifecycle::refusalAfterASessionStillRetries()
{
    loginAndWait();
    QSignalSpy failed(connection, &CalaosConnection::loginFailed);
    server->acceptedPass = QStringLiteral("changed");
    server->attempts.clear();
    networkDown();
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Reconnecting);
    QCOMPARE(failed.size(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(server->attempts.size() == ReconnectPolicy::MaxAuthFailures, 15000);
    QTRY_COMPARE(failed.size(), 1);
}

//...and a launch login deferred offline, even with a password never accepted.
void TstSessionLifecycle::offlineLaunchRefusalStillRetries()
{
    QSignalSpy failed(connection, &CalaosConnection::loginFailed);
    networkDown();
    QVERIFY(launchLoginOffline(TypedUser, TypedPass));
    networkUp();
    QTRY_VERIFY_WITH_TIMEOUT(server->attempts.size() == ReconnectPolicy::MaxAuthFailures, 15000);
    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(server->attempts.size(), ReconnectPolicy::MaxAuthFailures);
}

//iOS: the same once the connection gave up, i.e. after the "Login failed"
//alert: returning neither retries nor brings the alert back.
void TstSessionLifecycle::iosReturnAfterAGiveUpDoesNotReplay()
{
    QSignalSpy failed(connection, &CalaosConnection::loginFailed);
    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(server->attempts.size(), 1);

    QVERIFY(returnsReachNoServer(false));
    QCOMPARE(failed.size(), 1);
}

//iOS: the stop lasts until a login goes through; then a session is back and
//the return logs in again as usual.
void TstSessionLifecycle::iosReturnWorksAgainAfterASuccessfulLogin()
{
    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QVERIFY(returnsReachNoServer(false));

    loginAndWait();
    appBackground();
    QVERIFY(appForeground());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);
}

//No login on return means nothing will bring the kept page back to life.
void TstSessionLifecycle::iosReturnDropsTheKeptPageWhenItDoesNotLogIn()
{
    loginAndWait();
    connection->logout();
    appBackground(false, true);
    QVERIFY(resumePolicy.keepPage());
    QVERIFY(!appForeground());
    QVERIFY(!resumePolicy.keepPage());

    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    appBackground(false, true);
    QVERIFY(resumePolicy.keepPage());
    QVERIFY(!appForeground());
    QVERIFY(!resumePolicy.keepPage());
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
    //Rejected: given up at once, nothing is wanted any more.
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QVERIFY(connection->failingOnCredentials());

    //The network changing starts nothing, with any credentials.
    networkDown();
    networkUp();
    settle();
    QCOMPARE(server->logins, 0);
    QCOMPARE(server->attempts, QStringList{ typed });
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
//A cold restart after a logout: see logoutSurvivesOfflineRestart() (FIX-11).
void TstSessionLifecycle::logoutThenOfflineAutoLoginStaysLoggedOut()
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

//FIX-11, the ticket's case: Logout, kill the app, launch it again. The login
//screen shows the stored credentials (they are not touched) and nothing logs
//in: not the launch timer, not iOS become-active, not the network.
void TstSessionLifecycle::logoutSurvivesRestart()
{
    loginAndWait();
    sessionUp();
    userLogout();
    QCOMPARE(storedSessionState(), QVariant(false));

    restart();
    QVERIFY(connection->sessionClosedByUser());
    QVERIFY(!launchLogin());
    QVERIFY(!appForeground()); //iOS reports become-active at launch too
    networkDown();
    networkUp();
    QVERIFY(!connection->acceptsImmediateReconnect()); //desktop discovery
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    //Restarted again without logging in: still logged out.
    restart();
    QVERIFY(!launchLogin());
    settle();
    QCOMPARE(server->logins, 1);
}

//FIX-11, offline variant: launched in airplane mode after a logout, the
//launch login is not recorded either, so the network coming back does
//nothing.
void TstSessionLifecycle::logoutSurvivesOfflineRestart()
{
    loginAndWait();
    sessionUp();
    userLogout();

    restart();
    networkDown();
    QVERIFY(!LaunchLogin::shouldAutoLogin(connection, TestUser, TestPass, server->baseUrl()));
    QVERIFY(!launchLoginOffline(TestUser, TestPass)); //deferLogin() refuses it too
    networkUp();
    settle();

    QCOMPARE(server->logins, 1);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
}

//FIX-11: Login by hand after the restart clears the state; the following
//restarts log in on their own again.
void TstSessionLifecycle::loginAfterRestartClearsTheLogout()
{
    loginAndWait();
    sessionUp();
    userLogout();
    restart();
    QVERIFY(!launchLogin());

    //A rejected login does not clear it: homeLoaded() never ran.
    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_VERIFY(server->attempts.size() == 2);
    connection->logout(); //the user gives up on the login screen
    QCOMPARE(storedSessionState(), QVariant(false));
    restart();
    QVERIFY(!launchLogin());

    loginAndWait(); //Login tapped with the right credentials
    sessionUp();
    QCOMPARE(storedSessionState(), QVariant(true));

    restart();
    QVERIFY(!connection->sessionClosedByUser());
    QVERIFY(launchLogin());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 3);
}

//FIX-11: nothing stored (fresh install, or an update from a version without
//the state): the launch login runs, as before.
void TstSessionLifecycle::noStoredStateLaunchLogsIn()
{
    QVERIFY(!storedSessionState().isValid());
    QVERIFY(HardwareUtils::Instance()->isSessionActive());

    restart();
    QVERIFY(launchLogin());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 1);
}

//FIX-11: Reset all data goes back to the initial state: the logout is
//forgotten, the stored credentials are the demo account's, which is never
//logged in automatically (unchanged demo behaviour).
void TstSessionLifecycle::resetAllDataForgetsTheLogout()
{
    loginAndWait();
    sessionUp();
    userLogout();

    HardwareUtils::Instance()->resetAuthKeychain();
    QVERIFY(!storedSessionState().isValid());
    QVERIFY(HardwareUtils::Instance()->isSessionActive());

    restart();
    QVERIFY(!connection->sessionClosedByUser());
    QString user, pass;
    HardwareUtils::Instance()->loadAuthKeychain(user, pass);
    QCOMPARE(user, Common::getDemoUser());
    QVERIFY(!launchLogin(user, pass, Common::getDemoHost()));
    QVERIFY(!launchLogin(QStringLiteral("demo"), pass, Common::getDemoHost()));

    //A real account typed afterwards logs in automatically at the next launch.
    QVERIFY(launchLogin());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(server->logins, 2);
}

//FIX-10: launched in airplane mode, then the session flaps with the link.
//The banner is up exactly while the link is down and a session is wanted.
void TstSessionLifecycle::offlineBannerFollowsTheSessionIntent()
{
    //Nothing wanted yet (demo account on the login screen, offline): no banner.
    QVERIFY(!connection->waitsForNetwork(true));

    networkDown();
    QVERIFY(launchLoginOffline(TestUser, TestPass));
    QVERIFY(connection->waitsForNetwork(true));

    networkUp();
    QVERIFY(!connection->waitsForNetwork(false)); //hidden as soon as the link is back
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(!connection->waitsForNetwork(false));

    //The link drops during the session: shown during the outage only.
    networkDown();
    QVERIFY(connection->waitsForNetwork(true));
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(!connection->waitsForNetwork(false));
    QCOMPARE(server->logins, 2);
}

//FIX-10 + FIX-11: no banner after a deliberate logout, offline, even after a
//restart; Login tapped offline brings it (and the connection) back.
void TstSessionLifecycle::offlineBannerNotShownAfterLogout()
{
    loginAndWait();
    sessionUp();
    userLogout();

    networkDown();
    QVERIFY(!launchLoginOffline(TestUser, TestPass));
    QVERIFY(!connection->waitsForNetwork(true));

    restart();
    networkDown();
    QVERIFY(!launchLoginOffline(TestUser, TestPass));
    QVERIFY(!connection->waitsForNetwork(true));

    QVERIFY(typedLoginOffline(TestUser, TestPass));
    QVERIFY(connection->waitsForNetwork(true));
    networkUp();
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(!connection->waitsForNetwork(false));

    //Logged out again while offline: the banner goes away with the intent.
    networkDown();
    QVERIFY(connection->waitsForNetwork(true));
    userLogout();
    QVERIFY(!connection->waitsForNetwork(true));
}

//FIX-10: the credentials were given up on:
//the link coming back would not reconnect, so there is nothing to wait for.
void TstSessionLifecycle::offlineBannerNotShownAfterCredentialsGiveUp()
{
    connection->login(TypedUser, TypedPass, server->baseUrl());
    QTRY_COMPARE(server->attempts.size(), 1);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    networkDown();
    QVERIFY(!connection->waitsForNetwork(true));
    QVERIFY(!launchLoginOffline(TestUser, TestPass)); //an automatic login does not lift the give-up
    QVERIFY(!connection->waitsForNetwork(true));
}

QTEST_MAIN(TstSessionLifecycle)
#include "tst_sessionlifecycle.moc"
