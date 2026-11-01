//Unit tests for ReconnectPolicy (T14).
//
//ReconnectPolicy is the decision half of the reconnection logic: it holds the
//state machine, the exponential backoff, the anti double-login guard, the
//authentication give-up rule and the long-poll tolerance. CalaosConnection is
//only the execution half (one QTimer, the sockets, the HTTP requests).
//
//That split exists so this file can prove the behaviour the ticket asks for
//without a calaos_server and without a single socket: everything below is
//pure state transitions.
//
//What this file does NOT prove is the wiring: that CalaosConnection really
//calls beginAttempt() before opening a socket, arms its single QTimer with
//pendingDelayMs(), and routes a websocket error and a poll error to the right
//failure kind. That part stays a manual verification against a real server.

#include <QtTest>
#include <QSet>

#include "CalaosConnection.h"

class TstReconnectPolicy: public QObject
{
    Q_OBJECT

private slots:
    void backoffSequenceIsExact();
    void baseDelayIsCapped_data();
    void baseDelayIsCapped();
    void jitterStaysWithinBoundsAndUnderTheCap();
    void jitterStillSpreadsAtTheCeiling();

    void neverTwoAttemptsInFlight();
    void secondFailureReportDoesNotArmASecondRetry();
    void successResetsTheBackoff();
    void stateWalkCoversEveryState();

    void transientFailuresNeverGiveUp();
    void wrongPasswordStopsRetrying();
    void wrongPasswordStillAllowsAManualLogin();
    void wrongPasswordOnTheLoginScreenGivesUpAtOnce();
    void authFailureCounterIsResetByASuccess();
    void untrustedCertificateNeverRetriesByItself();

    void singlePollFailureKeepsTheSession();
    void pollFailureCounterIsResetByASuccessfulPoll();

    void logoutForgetsTheIntent();
    void suspendKeepsTheIntent();

    //FIX-7: a session closed on purpose stays closed.
    void logoutThenNetworkFlapDoesNotReconnect();
    void logoutDuringBackoffThenNetworkFlapDoesNotReconnect();
    void logoutBlocksEveryAutomaticAttempt();
    void loginAfterLogoutWorks();
    void activeSessionSurvivesRepeatedNetworkFlaps();
    void backgroundBlocksTheNetworkComingBack();
    void backgroundThenForegroundResumesAWantedSession();
    void logoutThenBackgroundThenForegroundStaysLoggedOut();
    void backgroundBeforeAnyLoginWantsNothing();

    void offlineLoginRecordsTheIntent();

private:
    //One complete failing attempt: start it, then report the failure.
    //Returns what the policy answered, ie "arm the retry timer".
    static bool attemptAndFail(ReconnectPolicy &p,
                               ReconnectPolicy::Failure f = ReconnectPolicy::TransientFailure,
                               ReconnectPolicy::Trigger t = ReconnectPolicy::AutoRetry)
    {
        if (!p.canBeginAttempt(t))
            return false;
        p.beginAttempt(t);
        return p.failed(f);
    }
};

//The headline acceptance criterion: 1 / 2 / 4 / 8 / 16 / 30 / 30 s.
void TstReconnectPolicy::backoffSequenceIsExact()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    const QList<int> expected = { 1000, 2000, 4000, 8000, 16000, 30000, 30000, 30000 };

    //First failure comes from a user initiated login, the following ones from
    //the retry timer, exactly like the real flow.
    QVERIFY(attemptAndFail(p, ReconnectPolicy::TransientFailure, ReconnectPolicy::UserRequest));
    QCOMPARE(p.pendingDelayMs(), expected.at(0));
    QCOMPARE(p.failureCount(), 1);

    for (int i = 1; i < expected.size(); i++)
    {
        QVERIFY2(attemptAndFail(p), qPrintable(QStringLiteral("attempt %1 refused").arg(i)));
        QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
        QCOMPARE(p.failureCount(), i + 1);
        QCOMPARE(p.pendingDelayMs(), expected.at(i));
    }
}

void TstReconnectPolicy::baseDelayIsCapped_data()
{
    QTest::addColumn<int>("failureCount");
    QTest::addColumn<int>("expected");

    //Out of range values must be clamped, not shifted out of an int.
    QTest::newRow("zero")      << 0   << 1000;
    QTest::newRow("negative")  << -5  << 1000;
    QTest::newRow("1")         << 1   << 1000;
    QTest::newRow("2")         << 2   << 2000;
    QTest::newRow("3")         << 3   << 4000;
    QTest::newRow("4")         << 4   << 8000;
    QTest::newRow("5")         << 5   << 16000;
    QTest::newRow("6")         << 6   << 30000;
    QTest::newRow("7")         << 7   << 30000;
    QTest::newRow("100")       << 100 << 30000;
    QTest::newRow("1000000")   << 1000000 << 30000;
}

void TstReconnectPolicy::baseDelayIsCapped()
{
    QFETCH(int, failureCount);
    QFETCH(int, expected);

    QCOMPARE(ReconnectPolicy::baseDelayMsForFailure(failureCount), expected);
}

//A long outage spends all of its time at the ceiling, so that is where the
//jitter has to work. Adding to 30000 and clamping used to land half the draws
//on exactly 30000, spreading nothing at all: measured at 49.9% over 100k
//samples against a real client. The delay must stay a real distribution.
void TstReconnectPolicy::jitterStillSpreadsAtTheCeiling()
{
    ReconnectPolicy p;
    QVERIFY(p.isJitterEnabled());

    const int cap = ReconnectPolicy::MaxDelayMs;
    const int span = cap * ReconnectPolicy::JitterPercent / 100;

    int atCap = 0;
    int distinct = 0;
    QSet<int> seen;
    const int samples = 2000;

    for (int i = 0; i < samples; i++)
    {
        p.stop();
        //six consecutive failures put the backoff at the ceiling
        for (int n = 1; n <= 6; n++)
            attemptAndFail(p, ReconnectPolicy::TransientFailure,
                           n == 1 ? ReconnectPolicy::UserRequest
                                  : ReconnectPolicy::AutoRetry);

        const int delay = p.pendingDelayMs();
        QVERIFY2(delay <= cap, qPrintable(QStringLiteral("delay %1 over the ceiling").arg(delay)));
        QVERIFY2(delay >= cap - span,
                 qPrintable(QStringLiteral("delay %1 below the -20%% bound").arg(delay)));
        if (delay == cap)
            atCap++;
        seen.insert(delay);
    }

    distinct = seen.size();

    //With the clamp bug this was ~50%. A correct downward-only jitter puts a
    //single draw on the exact ceiling, so well under a tenth of the samples.
    QVERIFY2(atCap * 10 < samples,
             qPrintable(QStringLiteral("%1 of %2 draws landed on exactly %3 ms")
                        .arg(atCap).arg(samples).arg(cap)));
    QVERIFY2(distinct > samples / 10,
             qPrintable(QStringLiteral("only %1 distinct delays over %2 draws")
                        .arg(distinct).arg(samples)));
}

void TstReconnectPolicy::jitterStaysWithinBoundsAndUnderTheCap()
{
    ReconnectPolicy p;
    QVERIFY(p.isJitterEnabled()); //jitter is on by default in production

    //Walk the whole sequence many times: every jittered delay must stay within
    //+-20% of its base value, must never be zero or negative, and must never
    //push the ceiling above the advertised 30s.
    for (int round = 0; round < 200; round++)
    {
        p.stop();
        for (int i = 1; i <= 8; i++)
        {
            const bool retry = attemptAndFail(p,
                                              ReconnectPolicy::TransientFailure,
                                              i == 1 ? ReconnectPolicy::UserRequest
                                                     : ReconnectPolicy::AutoRetry);
            QVERIFY(retry);

            const int base = ReconnectPolicy::baseDelayMsForFailure(i);
            const int span = base * ReconnectPolicy::JitterPercent / 100;
            const int delay = p.pendingDelayMs();

            QVERIFY2(delay >= base - span,
                     qPrintable(QStringLiteral("delay %1 below base %2 - 20%%").arg(delay).arg(base)));
            QVERIFY2(delay <= base + span,
                     qPrintable(QStringLiteral("delay %1 above base %2 + 20%%").arg(delay).arg(base)));
            QVERIFY(delay > 0);
            QVERIFY2(delay <= ReconnectPolicy::MaxDelayMs,
                     qPrintable(QStringLiteral("delay %1 above the 30s ceiling").arg(delay)));
        }
    }
}

//The whole point of the ticket: there is never more than one attempt in flight.
void TstReconnectPolicy::neverTwoAttemptsInFlight()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
    p.beginAttempt(ReconnectPolicy::UserRequest);
    QCOMPARE(p.state(), ReconnectPolicy::Connecting);

    //Every other path that used to schedule its own singleShot(1000, login):
    //the disconnected handler, the loginFailed handler and server discovery.
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::UserRequest));

    p.attemptSucceeded();
    QCOMPARE(p.state(), ReconnectPolicy::Connected);
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::UserRequest));

    //Only once the session is really gone may a new attempt start.
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
}

//A websocket that breaks emits error() AND disconnected(); a poll that dies
//while the ping timeout fires reports twice too. The second report must be
//inert, otherwise two retry timers (and then two logins) are armed.
void TstReconnectPolicy::secondFailureReportDoesNotArmASecondRetry()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    p.beginAttempt(ReconnectPolicy::UserRequest);
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.failureCount(), 1);
    QCOMPARE(p.pendingDelayMs(), 1000);

    //Same failure reported again by the sibling signal.
    QVERIFY(!p.failed(ReconnectPolicy::TransientFailure));
    QVERIFY(!p.failed(ReconnectPolicy::AuthFailure));
    QCOMPARE(p.failureCount(), 1);
    QCOMPARE(p.pendingDelayMs(), 1000);
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
    QVERIFY(!p.hasGivenUp());
}

void TstReconnectPolicy::successResetsTheBackoff()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    for (int i = 1; i <= 5; i++)
        QVERIFY(attemptAndFail(p));
    QCOMPARE(p.failureCount(), 5);
    QCOMPARE(p.pendingDelayMs(), 16000);

    //Server is back.
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    p.attemptSucceeded();

    QCOMPARE(p.failureCount(), 0);
    QCOMPARE(p.pendingDelayMs(), 0);
    QCOMPARE(p.state(), ReconnectPolicy::Connected);

    //Next outage starts again at 1s, not at 30s.
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.pendingDelayMs(), 1000);
}

void TstReconnectPolicy::stateWalkCoversEveryState()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    p.beginAttempt(ReconnectPolicy::UserRequest);
    QCOMPARE(p.state(), ReconnectPolicy::Connecting);
    p.attemptSucceeded();
    QCOMPARE(p.state(), ReconnectPolicy::Connected);
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    QCOMPARE(p.state(), ReconnectPolicy::Connecting);
    p.attemptSucceeded();
    QCOMPARE(p.state(), ReconnectPolicy::Connected);
    p.stop();
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
}

//Server down for two minutes must keep retrying, forever, at 30s.
void TstReconnectPolicy::transientFailuresNeverGiveUp()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    for (int i = 0; i < 500; i++)
    {
        QVERIFY(attemptAndFail(p));
        QVERIFY(!p.hasGivenUp());
        QVERIFY(p.pendingDelayMs() <= ReconnectPolicy::MaxDelayMs);
    }
    QCOMPARE(p.pendingDelayMs(), 30000);
    QCOMPARE(p.authFailureCount(), 0);
}

//Wrong password: bounded retries, then the automatic paths are locked out.
void TstReconnectPolicy::wrongPasswordStopsRetrying()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    //A session was up; its reconnection is refused.
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));

    QVERIFY(attemptAndFail(p, ReconnectPolicy::AuthFailure));
    QCOMPARE(p.authFailureCount(), 1);
    QVERIFY(!p.hasGivenUp());

    for (int i = 2; i < ReconnectPolicy::MaxAuthFailures; i++)
    {
        QVERIFY(attemptAndFail(p, ReconnectPolicy::AuthFailure));
        QCOMPARE(p.authFailureCount(), i);
        QVERIFY(!p.hasGivenUp());
    }

    //The last one gives up: no timer is armed and the state is terminal.
    QVERIFY(!attemptAndFail(p, ReconnectPolicy::AuthFailure));
    QCOMPARE(p.authFailureCount(), ReconnectPolicy::MaxAuthFailures);
    QVERIFY(p.hasGivenUp());
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QCOMPARE(p.pendingDelayMs(), 0);

    //Every automatic path is now refused: the retry timer, server discovery
    //and the network coming back. This is the "no infinite retry" guarantee.
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(!p.canResume());
    QVERIFY(!p.wantsConnection());
}

void TstReconnectPolicy::wrongPasswordOnTheLoginScreenGivesUpAtOnce()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    QVERIFY(!attemptAndFail(p, ReconnectPolicy::AuthFailure, ReconnectPolicy::UserRequest));
    QVERIFY(p.hasGivenUp());
    QCOMPARE(p.authFailureCount(), 1);
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QCOMPARE(p.pendingDelayMs(), 0);

    //Still before any accepted login: a server down first, refused next.
    QVERIFY(attemptAndFail(p, ReconnectPolicy::TransientFailure, ReconnectPolicy::UserRequest));
    QVERIFY(!attemptAndFail(p, ReconnectPolicy::AuthFailure));
    QVERIFY(p.hasGivenUp());
}

void TstReconnectPolicy::wrongPasswordStillAllowsAManualLogin()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    attemptAndFail(p, ReconnectPolicy::AuthFailure, ReconnectPolicy::UserRequest);
    QVERIFY(p.hasGivenUp());

    //The user retypes the password on the login screen: that must work, and it
    //must clear the give-up and the backoff.
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
    p.beginAttempt(ReconnectPolicy::UserRequest);
    QVERIFY(!p.hasGivenUp());
    QCOMPARE(p.failureCount(), 0);
    QCOMPARE(p.authFailureCount(), 0);
    QCOMPARE(p.state(), ReconnectPolicy::Connecting);
}

void TstReconnectPolicy::authFailureCounterIsResetByASuccess()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QVERIFY(p.failed(ReconnectPolicy::AuthFailure));
    QCOMPARE(p.authFailureCount(), 1);

    p.beginAttempt(ReconnectPolicy::AutoRetry);
    p.attemptSucceeded();
    QCOMPARE(p.authFailureCount(), 0);

    //A rejection much later must start counting from scratch.
    QVERIFY(p.failed(ReconnectPolicy::AuthFailure));
    QCOMPARE(p.authFailureCount(), 1);
    QVERIFY(!p.hasGivenUp());
}

//An unknown server certificate waits for the user: a retry, the network
//coming back or server discovery must not ask again on their own.
void TstReconnectPolicy::untrustedCertificateNeverRetriesByItself()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    QVERIFY(!attemptAndFail(p, ReconnectPolicy::TrustFailure, ReconnectPolicy::UserRequest));
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QVERIFY(p.hasGivenUp());
    QVERIFY(!p.wantsConnection());
    QVERIFY(!p.canResume());
    QCOMPARE(p.pendingDelayMs(), 0);
    //Not a network failure: no "connection lost" alert on the desktop.
    QCOMPARE(p.failureCount(), 0);
    QCOMPARE(p.authFailureCount(), 0);
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));

    //The user's answer (or a new login) starts again.
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QVERIFY(!p.hasGivenUp());
}

//The defect users see most: one transient long-poll error emptied the UI.
void TstReconnectPolicy::singlePollFailureKeepsTheSession()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    for (int i = 1; i < ReconnectPolicy::MaxPollFailures; i++)
    {
        QVERIFY2(p.pollFailed(),
                 qPrintable(QStringLiteral("poll failure %1 tore the session down").arg(i)));
        //Session untouched: still connected, no reconnection scheduled.
        QCOMPARE(p.state(), ReconnectPolicy::Connected);
        QCOMPARE(p.failureCount(), 0);
        QCOMPARE(p.pollFailureCount(), i);
    }

    //Only the run of failures declares the link dead.
    QVERIFY(!p.pollFailed());
    QCOMPARE(p.pollFailureCount(), ReconnectPolicy::MaxPollFailures);

    //And that one goes through the normal transient path: backoff, no give-up.
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
    QVERIFY(!p.hasGivenUp());
}

void TstReconnectPolicy::pollFailureCounterIsResetByASuccessfulPoll()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    //An installation that hiccups once every hour must never reach the
    //threshold, however long it runs.
    for (int i = 0; i < 100; i++)
    {
        QVERIFY(p.pollFailed());
        QCOMPARE(p.pollFailureCount(), 1);
        p.pollSucceeded();
        QCOMPARE(p.pollFailureCount(), 0);
    }
    QCOMPARE(p.state(), ReconnectPolicy::Connected);
}

void TstReconnectPolicy::logoutForgetsTheIntent()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QVERIFY(p.wantsConnection());

    p.stop();
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QVERIFY(!p.wantsConnection());
    //Nothing must bring the session back on its own after an explicit logout.
    QVERIFY(!p.canResume());
    QCOMPARE(p.failureCount(), 0);
    QCOMPARE(p.pendingDelayMs(), 0);
}

void TstReconnectPolicy::suspendKeepsTheIntent()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    //Airplane mode on: session dropped, but we still want one.
    p.suspend();
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QVERIFY(p.wantsConnection());
    QVERIFY(p.canResume());
    QCOMPARE(p.failureCount(), 0);

    //Airplane mode off: exactly one attempt, and the backoff starts fresh.
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    QVERIFY(!p.canResume()); //not twice
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.pendingDelayMs(), 1000);
}

//The ticket's case: Settings -> Logout, then airplane mode on and off.
//Application calls stop(), then suspend() on Disconnected and asks
//canResume() on Online: the answer must be no, every time.
void TstReconnectPolicy::logoutThenNetworkFlapDoesNotReconnect()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    p.stop();
    QVERIFY(p.isStopped());
    for (int i = 0; i < 3; i++)
    {
        p.suspend();
        QVERIFY(!p.canResume());
        QVERIFY(!p.wantsConnection());
        QVERIFY(p.isStopped());
    }
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
}

//Same, while a retry was pending: the logout wins over the backoff.
void TstReconnectPolicy::logoutDuringBackoffThenNetworkFlapDoesNotReconnect()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);

    p.stop();
    p.suspend();
    QVERIFY(!p.canResume());
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
}

//The network coming back is not the only automatic trigger: the retry timer
//and the desktop server discovery go through canBeginAttempt(AutoRetry).
void TstReconnectPolicy::logoutBlocksEveryAutomaticAttempt()
{
    ReconnectPolicy p;
    //Before anything happened, discovery may start the first session (that
    //is how the desktop logs in).
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::AutoRetry));

    p.beginAttempt(ReconnectPolicy::AutoRetry);
    p.attemptSucceeded();
    p.stop();
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
    //A failure report arriving after the logout arms nothing.
    QVERIFY(!p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
}

void TstReconnectPolicy::loginAfterLogoutWorks()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    p.stop();

    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
    p.beginAttempt(ReconnectPolicy::UserRequest);
    QVERIFY(!p.isStopped());
    QVERIFY(p.wantsConnection());
    p.attemptSucceeded();
    QCOMPARE(p.state(), ReconnectPolicy::Connected);

    //And that new session is back to the normal rules: a network flap
    //reconnects it.
    p.suspend();
    QVERIFY(p.canResume());
}

//Kept behaviour (T14): an active session that loses the link reconnects when
//it comes back, as many times as it happens.
void TstReconnectPolicy::activeSessionSurvivesRepeatedNetworkFlaps()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    for (int i = 0; i < 3; i++)
    {
        p.suspend();
        QVERIFY(p.canResume());
        //Two "Online" in a row (Android reports reachability and medium):
        //only one attempt.
        p.beginAttempt(ReconnectPolicy::AutoRetry);
        QVERIFY(!p.canResume());
        p.attemptSucceeded();
    }

    //Link lost while the attempt is in flight: still wanted.
    p.suspend();
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    p.suspend();
    QVERIFY(p.canResume());
}

//iOS resign-active: the session is dropped, and a Reachability change seen
//while in the background must not reconnect.
void TstReconnectPolicy::backgroundBlocksTheNetworkComingBack()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    p.pause();
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QVERIFY(p.isPaused());
    QVERIFY(p.wantsConnection());
    QVERIFY(!p.canResume());

    p.suspend();
    QVERIFY(!p.canResume());
    QVERIFY(p.isPaused());
    QVERIFY(!p.canBeginAttempt(ReconnectPolicy::AutoRetry));
}

//iOS become-active: the session that was up when the app left comes back.
void TstReconnectPolicy::backgroundThenForegroundResumesAWantedSession()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();

    p.pause();
    p.unpause();
    QVERIFY(!p.isPaused());
    QVERIFY(!p.isStopped()); //Application logs in again
    QVERIFY(p.canResume());
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    QCOMPARE(p.state(), ReconnectPolicy::Connected);

    //Background while an attempt was in flight (the launch login racing a
    //permission prompt): same thing.
    p.pause();
    p.unpause();
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.pause();
    p.unpause();
    QVERIFY(!p.isStopped());
    QVERIFY(p.canResume());
}

//iOS: Logout, switch to another app, come back. The logout must hold.
void TstReconnectPolicy::logoutThenBackgroundThenForegroundStaysLoggedOut()
{
    ReconnectPolicy p;
    p.beginAttempt(ReconnectPolicy::UserRequest);
    p.attemptSucceeded();
    p.stop();

    p.pause();
    p.suspend();
    QVERIFY(!p.canResume());
    p.unpause();
    QVERIFY(p.isStopped()); //Application does not log in again
    QVERIFY(!p.canResume());

    //The login screen still works.
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
}

//iOS reports become-active at launch too, with nothing paused: the policy
//neither wants nor forbids a session, Application's launch login applies.
void TstReconnectPolicy::backgroundBeforeAnyLoginWantsNothing()
{
    ReconnectPolicy p;
    p.unpause();
    QVERIFY(!p.isStopped());
    QVERIFY(!p.canResume());

    p.pause();
    p.unpause();
    QVERIFY(!p.isStopped());
    QVERIFY(!p.canResume());
    QVERIFY(p.canBeginAttempt(ReconnectPolicy::UserRequest));
}

//FIX-9: a login refused for lack of network starts nothing but makes the
//session wanted; an automatic one never reopens a closed or given-up
//session, an explicit one does.
void TstReconnectPolicy::offlineLoginRecordsTheIntent()
{
    ReconnectPolicy p;
    p.setJitterEnabled(false);

    //Launch in airplane mode.
    QVERIFY(p.deferAttempt(ReconnectPolicy::AutoRetry));
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QVERIFY(p.wantsConnection());
    QVERIFY(p.canResume());
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    QVERIFY(!p.canResume());
    //Nothing deferred while an attempt is in flight or a session is up.
    QVERIFY(!p.deferAttempt(ReconnectPolicy::UserRequest));
    p.attemptSucceeded();
    QVERIFY(!p.deferAttempt(ReconnectPolicy::UserRequest));
    QCOMPARE(p.state(), ReconnectPolicy::Connected);

    //Logout: an automatic offline login changes nothing, a tap on Login does.
    p.stop();
    QVERIFY(!p.deferAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(p.isStopped());
    QVERIFY(!p.canResume());
    QVERIFY(p.deferAttempt(ReconnectPolicy::UserRequest));
    QVERIFY(!p.isStopped());
    QVERIFY(p.canResume());

    //Given up on the credentials: same rule.
    for (int i = 0; i < ReconnectPolicy::MaxAuthFailures; i++)
    {
        p.beginAttempt(ReconnectPolicy::AutoRetry);
        p.failed(ReconnectPolicy::AuthFailure);
    }
    QVERIFY(p.hasGivenUp());
    QVERIFY(!p.deferAttempt(ReconnectPolicy::AutoRetry));
    QVERIFY(!p.canResume());
    QVERIFY(p.deferAttempt(ReconnectPolicy::UserRequest));
    QVERIFY(!p.hasGivenUp());
    QCOMPARE(p.authFailureCount(), 0);
    QVERIFY(p.canResume());

    //A pending retry is replaced, not kept armed.
    p.beginAttempt(ReconnectPolicy::AutoRetry);
    QVERIFY(p.failed(ReconnectPolicy::TransientFailure));
    QCOMPARE(p.state(), ReconnectPolicy::Reconnecting);
    QVERIFY(p.deferAttempt(ReconnectPolicy::AutoRetry));
    QCOMPARE(p.state(), ReconnectPolicy::Disconnected);
    QCOMPARE(p.pendingDelayMs(), 0);

    //It does not lift an iOS pause: only the foreground does.
    p.pause();
    QVERIFY(p.deferAttempt(ReconnectPolicy::UserRequest));
    QVERIFY(p.isPaused());
    QVERIFY(!p.canResume());
    p.unpause();
    QVERIFY(p.canResume());
}

QTEST_APPLESS_MAIN(TstReconnectPolicy)
#include "tst_reconnectpolicy.moc"
