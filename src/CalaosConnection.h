#ifndef CALAOSCONNECTION_H
#define CALAOSCONNECTION_H

#include <QtCore>
#include <QtNetwork>
#include <QRandomGenerator>
#include <QWebSocket>

#include "CalaosEventDecoder.h"
#include "IOConnection.h"
#include "JsonKeys.h"
#include "TlsPinning.h"

//Reconnection state machine and exponential backoff (T14).
//
//This class only *decides*: it holds no QObject, no QTimer and no network
//code. CalaosConnection *executes* those decisions with the single
//reconnection QTimer it owns. That split is what makes the whole reconnection
//behaviour testable without a server, and it is why the class is header-only:
//a unit test can include it without linking the network stack (see
//tests/tst_reconnectpolicy).
//
//It is also the single source of truth for the anti double-login guard: an
//attempt may only start when no other attempt is in flight, which used to be
//guarded twice (Application::applicationStatus and CalaosConnection::constate)
//and could desynchronise.
class ReconnectPolicy
{
public:
    enum State
    {
        Disconnected,  //idle: nothing in flight, no retry armed
        Connecting,    //exactly one attempt in flight
        Connected,     //session up
        Reconnecting,  //waiting for the backoff delay, nothing in flight
    };

    enum Failure
    {
        TransientFailure, //network or server hiccup: retry with backoff
        AuthFailure,      //credentials rejected: bounded number of retries
        //Server certificate not trusted: only the user can change that
        //(confirm it, or log in again), so nothing retries on its own.
        TrustFailure,
    };

    enum Trigger
    {
        //Explicit login (login screen, launch login and iOS become-active
        //when online): resets the backoff and any give-up.
        UserRequest,
        //Backoff timer, server discovery, network coming back, and the
        //automatic logins (launch, iOS become-active) deferred offline
        //(FIX-9). Become-active is deliberately UserRequest online but
        //AutoRetry offline: a credentials give-up is not undone offline.
        AutoRetry,
    };

    //Backoff: 1s, 2s, 4s, 8s, 16s, then capped at 30s.
    static constexpr int BaseDelayMs = 1000;
    static constexpr int MaxDelayMs = 30000;
    //+-20% jitter so a fleet of clients does not hammer a server that just
    //came back in lockstep.
    static constexpr int JitterPercent = 20;
    //Wrong credentials will not fix themselves: after that many rejections we
    //stop retrying and hand over to the login screen.
    static constexpr int MaxAuthFailures = 3;
    //A single long-poll error is not a disconnection. Only a run of them is.
    static constexpr int MaxPollFailures = 3;

    State state() const { return m_state; }
    int failureCount() const { return m_failures; }
    int authFailureCount() const { return m_authFailures; }
    int pollFailureCount() const { return m_pollFailures; }
    bool hasGivenUp() const { return m_givenUp; }
    bool wantsConnection() const { return m_wantsConnection; }
    //The session was deliberately closed (logout, login cancelled): only an
    //explicit login may open one again (FIX-7).
    bool isStopped() const { return m_stopped; }
    //The application is in the background (iOS resign-active): nothing may
    //reconnect on its own until it comes back (FIX-7).
    bool isPaused() const { return m_paused; }
    //Delay decided for the retry the caller must arm, in ms.
    int pendingDelayMs() const { return m_pendingDelayMs; }

    //Tests disable the jitter to assert the exact 1/2/4/8/16/30/30 sequence.
    void setJitterEnabled(bool en) { m_jitterEnabled = en; }
    bool isJitterEnabled() const { return m_jitterEnabled; }

    //Backoff without jitter for the n-th consecutive failure (n starts at 1).
    static int baseDelayMsForFailure(int failureCount)
    {
        if (failureCount < 1) failureCount = 1;
        if (failureCount > 24) failureCount = 24; //never shift out of range
        const qint64 d = qint64(BaseDelayMs) << (failureCount - 1);
        return d > MaxDelayMs ? MaxDelayMs : int(d);
    }

    //THE anti double-login guard: one attempt at a time, ever.
    //It is also the guard of every automatic attempt (retry timer, network
    //coming back, server discovery): none may reopen a session that was given
    //up on, deliberately closed, or that belongs to an app in the background.
    bool canBeginAttempt(Trigger t) const
    {
        if (m_state == Connecting || m_state == Connected)
            return false;
        if (t == AutoRetry && (m_givenUp || m_stopped || m_paused))
            return false;
        return true;
    }

    //Commits the attempt. Only call it when canBeginAttempt() said yes.
    void beginAttempt(Trigger t)
    {
        if (t == UserRequest)
        {
            //The user just handed us (possibly new) credentials: forget the
            //past failures and the give-up decision.
            m_failures = 0;
            m_authFailures = 0;
            m_givenUp = false;
            //An explicit login reopens a deliberately closed session. It does
            //not lift a pause: only the app coming back to the foreground does.
            m_stopped = false;
        }
        m_pollFailures = 0;
        m_pendingDelayMs = 0;
        m_wantsConnection = true;
        m_state = Connecting;
    }

    void attemptSucceeded()
    {
        m_failures = 0;
        m_authFailures = 0;
        m_pollFailures = 0;
        m_givenUp = false;
        m_pendingDelayMs = 0;
        m_wantsConnection = true;
        m_state = Connected;
    }

    //Returns true when the caller must arm the retry timer with
    //pendingDelayMs(). Called again from Reconnecting or Disconnected it is a
    //no-op returning false: that is what keeps a websocket which errors *and*
    //then closes from arming two timers and starting two logins.
    bool failed(Failure f)
    {
        if (m_state != Connecting && m_state != Connected)
            return false;

        if (f == TrustFailure)
        {
            //Not counted as a network failure: no "connection lost" alert,
            //and an automatic attempt must not prompt for the same
            //certificate again.
            m_givenUp = true;
            m_wantsConnection = false;
            m_pendingDelayMs = 0;
            m_pollFailures = 0;
            m_state = Disconnected;
            return false;
        }

        if (f == AuthFailure)
        {
            m_authFailures++;
            if (m_authFailures >= MaxAuthFailures)
            {
                //No infinite retry on a wrong password.
                m_givenUp = true;
                m_wantsConnection = false;
                m_pendingDelayMs = 0;
                m_pollFailures = 0;
                m_state = Disconnected;
                return false;
            }
        }

        m_failures++;
        m_pollFailures = 0;
        m_pendingDelayMs = computeDelayMs(m_failures);
        m_state = Reconnecting;
        return true;
    }

    //Deliberate close (Settings -> Logout, login cancelled, reset): forget
    //everything, including that a session was wanted. The session stays
    //closed whatever happens next (link lost and back, server discovered,
    //app sent to the background and back) until an explicit login.
    void stop()
    {
        m_state = Disconnected;
        m_failures = 0;
        m_authFailures = 0;
        m_pollFailures = 0;
        m_givenUp = false;
        m_pendingDelayMs = 0;
        m_wantsConnection = false;
        m_stopped = true;
    }

    //The link is known to be down (airplane mode, cable unplugged): drop the
    //session but remember whether one is wanted, so canResume() says yes when
    //the link comes back. It only ever drops: it never turns a stopped or
    //paused policy back into one that wants a session.
    void suspend()
    {
        m_state = Disconnected;
        m_failures = 0;
        m_pollFailures = 0;
        m_pendingDelayMs = 0;
    }

    //A login refused because the device has no network (FIX-9): nothing
    //starts, but the session is now wanted, so canResume() says yes when the
    //link comes back. UserRequest (login screen) counts as an explicit login:
    //it resets the backoff and the give-up and reopens a deliberately closed
    //session. AutoRetry (launch login, iOS become-active) does neither: it is
    //refused after a deliberate close or a give-up, so it can never bring
    //back a session the user closed. Like beginAttempt(), it does not lift a
    //pause. Returns false (nothing changed) when refused, or when an attempt
    //is in flight or a session is up.
    bool deferAttempt(Trigger t)
    {
        if (m_state == Connecting || m_state == Connected)
            return false;
        if (t == AutoRetry && (m_givenUp || m_stopped))
            return false;

        if (t == UserRequest)
        {
            m_failures = 0;
            m_authFailures = 0;
            m_givenUp = false;
            m_stopped = false;
        }
        m_pollFailures = 0;
        m_pendingDelayMs = 0;
        m_wantsConnection = true;
        m_state = Disconnected;
        return true;
    }

    //The application goes to the background (iOS resign-active): drop the
    //session like suspend(), and refuse every automatic attempt until
    //unpause(), so a network change seen in the background cannot reconnect.
    //Unlike stop() the intent is kept: if a session was wanted, the app
    //coming back to the foreground opens it again.
    void pause()
    {
        suspend();
        m_paused = true;
    }

    //The application is back in the foreground.
    void unpause() { m_paused = false; }

    //True when the app may reconnect on its own now: a session is wanted and
    //nothing (an attempt in flight, a give-up, a deliberate close, a pause)
    //forbids an automatic attempt.
    bool canResume() const
    {
        return m_wantsConnection && canBeginAttempt(AutoRetry);
    }

    //Long-poll tolerance. A single transient poll error used to log out and
    //wipe the eight UI models; only a run of them means the link is gone.
    void pollSucceeded() { m_pollFailures = 0; }
    //true  -> just restart the poll, keep the session and the UI as they are.
    //false -> the link is really gone, tear down and reconnect with backoff.
    bool pollFailed()
    {
        m_pollFailures++;
        return m_pollFailures < MaxPollFailures;
    }

private:
    int computeDelayMs(int failureCount) const
    {
        const int base = baseDelayMsForFailure(failureCount);
        if (!m_jitterEnabled)
            return base;

        const int span = base * JitterPercent / 100;
        //At the ceiling the jitter can only go down: adding to 30000 and then
        //clamping would land half the draws on exactly 30000 and spread
        //nothing, which is the regime a long outage spends all its time in and
        //precisely where a fleet must not come back in lockstep.
        const int high = base >= MaxDelayMs ? 1 : span + 1;
        const int delay = base + QRandomGenerator::global()->bounded(-span, high);
        return qBound(1, delay, MaxDelayMs);
    }

    State m_state = Disconnected;
    int m_failures = 0;
    int m_authFailures = 0;
    int m_pollFailures = 0;
    int m_pendingDelayMs = 0;
    bool m_givenUp = false;
    bool m_wantsConnection = false;
    bool m_stopped = false;
    bool m_paused = false;
    bool m_jitterEnabled = true;
};

//Decides when the push token must be (re)sent to calaos_server with
//"register_push" (FIX-3).
//
//The token is registered once per session, when the session is established,
//and again whenever the platform reports a new token while the session is up
//(FCM/APNs rotation, or a token that arrives after the login). A token that
//changes while no session is up is simply picked up by the next login, and
//the same token is never sent twice in one session.
//
//Header-only and free of QObject/network code for the same reason as
//ReconnectPolicy: tests/tst_pushnotif drives it without a server.
class PushRegistration
{
public:
    bool isSessionUp() const { return m_sessionUp; }
    QString registeredToken() const { return m_registered; }

    //The transport is logged in. Nothing was registered in this session yet.
    void sessionStarted()
    {
        m_sessionUp = true;
        m_registered.clear();
    }

    //The transport is gone (logout, lost connection, new attempt).
    void sessionEnded()
    {
        m_sessionUp = false;
        m_registered.clear();
    }

    //Called both at the start of a session and on every token change.
    //Returns true when register_push must be sent for this token now; the
    //token is then considered registered for the rest of the session.
    bool shouldRegister(const QString &token)
    {
        if (!m_sessionUp || token.isEmpty() || token == m_registered)
            return false;
        m_registered = token;
        return true;
    }

private:
    bool m_sessionUp = false;
    QString m_registered;
};

//The "data" object of register_push (AND-7).
//
//Android asks calaos_server for data-only FCM messages: the system never
//displays those itself, so CalaosMessagingService.onMessageReceived() runs
//in every app state (foreground, background, killed) and can post the
//notification with the camera picture. Without the field the server keeps
//the legacy notification+data format, which only reaches the app in the
//foreground (older app versions). iOS has its own APNs path and sends
//nothing. The contract with calaos_base is kept in this one place: the key
//is JsonKeys::PushFormat, the value AndroidPushFormat.
//
//JsonKeys::DeviceName lets the admin pick the approver phone among the
//registered tokens; an older calaos_server ignores it. Left out rather than
//sent empty when the platform has no name.
//
//Header-only so tests/tst_pushnotif checks the payload for each hardware
//without a Q_OS_ANDROID build.
namespace PushFormat
{
inline constexpr QLatin1StringView AndroidHardware{"android"};
inline constexpr QLatin1StringView AndroidPushFormat{"data"};

inline QJsonObject registration(const QString &token, const QString &hardware,
                                const QString &deviceName = QString())
{
    QJsonObject o = {
        { JsonKeys::Token, token },
        { JsonKeys::Hardware, hardware }
    };
    if (hardware == AndroidHardware)
        o[JsonKeys::PushFormat] = AndroidPushFormat;
    const QString name = deviceName.trimmed();
    if (!name.isEmpty())
        o[JsonKeys::DeviceName] = name;
    return o;
}
}

//QObject first, as Q_OBJECT requires; IOConnection is the read-only view the
//IO models are given, see src/IOConnection.h.
class CalaosConnection : public QObject, public IOConnection
{
    Q_OBJECT
public:
    explicit CalaosConnection(QObject *parent = 0);

    enum
    {
        ConStateUnknown,
        ConStateTryWebsocket,
        ConStateHttp,
        ConStateWebsocket,
    };

    //IOConnection: this object carries the io signals itself.
    QObject *eventSource() override { return this; }
    bool isHttpApiV2() const override { return isV2HttpApi; }

    //Reads the API version off a "get_home" payload. This is the criterion
    //RoomModel::load() used to apply room by room before writing the answer
    //back into this class through updateHttpApiV2() (T18): in the v2 shape a
    //room carries its items as a map with separate "inputs"/"outputs" lists,
    //before that "items" was a single flat list. It is decided once for the
    //whole payload - a single answer never mixes both shapes.
    //Returns false when the payload says nothing at all (no room: demo mode,
    //an empty home), in which case the current value must be kept.
    //Static and header-only on purpose: unit testable without the transport,
    //see tests/tst_httpapiversion.
    static bool detectHttpApiV2(const QVariantMap &home, bool &isV2)
    {
        const QVariantList rooms = home.value(JsonKeys::Home).toList();
        bool seenRoom = false;

        for (const QVariant &r: rooms)
        {
            const QVariantMap room = r.toMap();
            if (room.isEmpty())
                continue;

            seenRoom = true;
            if (room.value(JsonKeys::Items).toMap().contains(JsonKeys::Inputs))
            {
                isV2 = true;
                return true;
            }
        }

        if (!seenRoom)
            return false;

        isV2 = false;
        return true;
    }

    bool isWebsocket() { return wsocket && constate == ConStateWebsocket; }
    bool isHttp() { return constate == ConStateHttp; }

    //Sends a JSON action over the current transport, tagged with msgId over
    //the websocket (the server echoes it in its answer). Returns false, and
    //sends nothing, when no session is up (EVLOG-1: it used to drop the
    //request silently while the caller waited for an answer).
    bool sendJson(const QString &action, QJsonObject &jsonData,
                  const QString &msgId = QStringLiteral("user_cmd_json"));

    //Event log requests (EVLOG-1). Each one gets a unique id, also used as
    //msg_id over the websocket so the answer is tied back to its request.
    //An empty id means nothing was sent (no session up) and nothing will be
    //signalled. Otherwise exactly one eventLogPageFinished() (for a page) or
    //eventLogEventFinished() (for a single event) is emitted for that id:
    //with the server's answer, with the server's {"error": ...}, or as a
    //failure when the transport goes away first (logout, lost connection,
    //app sent to the background) or the HTTP request fails. The only
    //exception is cancelEventLogRequest(), after which nothing is emitted.
    QString requestEventLogPage(int page, int perPage);
    QString requestEventLogEvent(const QString &uuid);
    //The caller stopped waiting (timeout): forget the request, a late answer
    //is dropped.
    void cancelEventLogRequest(const QString &requestId);
    //Requests sent and not finished yet (tests).
    int pendingEventLogRequests() const { return pendingEventLog.size(); }

    QString getNotifPictureUrl(const QString &pic_uid);

    void changeCredentials(QString user, QString pass);

    //Lifecycle state, derived from the single ReconnectPolicy instance. This
    //is the only source of truth: Application mirrors it, it never keeps a
    //second flag of its own.
    ReconnectPolicy::State connectionState() const { return reconnectPolicy.state(); }

    //True while the current run of failures is the server rejecting our
    //credentials. Telling that apart matters for what we show the user: a
    //wrong password is not a network outage and will not fix itself.
    bool failingOnCredentials() const { return reconnectPolicy.authFailureCount() > 0; }
    int reconnectFailureCount() const { return reconnectPolicy.failureCount(); }

    //TLS trust on first use (TlsPinning.h). An attempt that meets a
    //certificate Qt does not trust and that is not pinned fails with
    //TrustFailure (no automatic retry) and asks the user once through
    //tlsTrustRequested(); the answer comes back here. Accepting pins the key
    //(or, for a CA-valid certificate, drops the pin of the host) and logs in
    //again, refusing keeps the session down.
    void answerTlsTrust(bool accept);
    bool hasPendingTlsTrust() const { return !pendingTrust.isEmpty(); }
    //A certificate refused in this run is refused silently afterwards (the
    //launch login, iOS become-active), so the prompt cannot come back in a
    //loop. An explicit login from the login screen asks again.
    void forgetTlsRefusals() { tlsRefused.clear(); }
    //Tests use a store of their own; default TlsPinStore::instance().
    void setPinStore(TlsPinStore *store);

    //Network discovery found a server: try again *now* instead of waiting for
    //the pending backoff delay. Never stacks a second timer or a second login,
    //the guard in ReconnectPolicy refuses the call when an attempt is already
    //in flight or when we gave up on the credentials.
    void requestImmediateReconnect(QString user, QString pass, QString host);
    //True when requestImmediateReconnect() would start an attempt right now.
    //Lets the caller skip reading the stored settings when nothing would use
    //them (FIX-8).
    bool acceptsImmediateReconnect() const;

    //The link is known to be down: drop the session but remember it is wanted.
    void suspend();
    //The link is back: reconnect immediately if a session was wanted, unless
    //it was deliberately closed or the app is in the background.
    //It reconnects with the credentials and host of the last attempt (explicit
    //login, accepted credentials change or desktop discovery) or of the last
    //login refused offline (deferLogin(), FIX-9), exactly like
    //the backoff timer does. Never with whatever the login
    //screen or the stored settings hold at that moment: a network change must
    //not log in with other credentials (FIX-8).
    void resume();
    //A login refused because the device has no network (FIX-9): start
    //nothing, but remember the session is wanted with these credentials and
    //host, so resume() connects with them when the link is back. The latest
    //recorded intent wins. trigger is UserRequest for the login screen and
    //AutoRetry for the automatic logins (launch, iOS become-active), which
    //never reopen a session closed by the user, see
    //ReconnectPolicy::deferAttempt(). Returns true when the intent was
    //recorded.
    bool deferLogin(QString user, QString pass, QString h, ReconnectPolicy::Trigger trigger);
    //The app goes to the background (iOS resign-active): drop the session,
    //keep the intent, and let nothing reconnect until unpause().
    void pause();
    //The app is back in the foreground. Reconnecting is up to the caller,
    //see sessionClosedByUser().
    void unpause();
    //The session was deliberately closed (logout() since the last explicit
    //login): coming back to the foreground must not log in again.
    bool sessionClosedByUser() const { return reconnectPolicy.isStopped(); }
    //What the offline banner shows (FIX-10): the link is down (linkDown, as
    //HardwareUtils reports it) while a session is wanted, i.e. one the link
    //coming back would open: a session lost with the network, or a login
    //refused offline and recorded by deferLogin(). Not after a logout (an
    //automatic login offline is not recorded then), nor once the credentials
    //were given up on: the network coming back would open nothing.
    bool waitsForNetwork(bool linkDown) const
    {
        return linkDown && reconnectPolicy.wantsConnection() &&
               !reconnectPolicy.isStopped() && !reconnectPolicy.hasGivenUp();
    }

private:
    QNetworkAccessManager *accessManager;

    //Only for queuing camera requests, on desktop max 6 // requests
    //This is to prevent blocking the main accessManager for the cameras
    QNetworkAccessManager *accessManagerCam;

    QString username, password;
    QString wshost, httphost;
    //Host string as it was given to login(), needed to retry on our own.
    QString lasthost;
    QString uuidPolling;

    //used when changing credentials
    QString username_temp, password_temp;

    int constate = ConStateUnknown;

    QList<QNetworkReply *> reqReplies;

    //Event log requests in flight, by request id (EVLOG-1).
    enum class EventLogKind { Page, Event };
    QHash<QString, EventLogKind> pendingEventLog;
    //HTTP: the reply that carries each of them (also listed in reqReplies).
    QHash<QNetworkReply *, QString> eventLogReplies;
    //Makes every request id unique for the lifetime of the connection, so a
    //late answer can never be taken for the answer to a newer request.
    quint64 eventLogSeq = 0;
    QString requestEventLog(EventLogKind kind, QJsonObject data);
    //Ends the request: emits its signal once, nothing for an unknown id
    //(already finished, cancelled, or the answer to a request of an older
    //session).
    void finishEventLogRequest(const QString &requestId, bool transportOk, const QVariantMap &answer);
    //Fails every request still in flight: their answer can no longer come.
    void failPendingEventLogRequests();
    //HTTP answer to an event log request.
    void eventLogReplyFinished(QNetworkReply *reply, const QString &requestId);
    QNetworkReply *pollReply;

    QWebSocket *wsocket = nullptr;

    bool isV2HttpApi = true;
    //Private since T18: the flag belongs to the transport and is set from the
    //payload the transport itself received (emitHomeLoaded), never pushed in
    //from a model.
    void updateHttpApiV2(bool en) { isV2HttpApi = en; }
    //detectHttpApiV2() on the incoming home, then emit homeLoaded(). Every
    //emitter of homeLoaded goes through here so the flag is always up to date
    //before any model reads the payload.
    void emitHomeLoaded(const QVariantMap &home);

    QTimer *wsPingTimeout = nullptr;
    QTimer *wsPing = nullptr;

    bool demoMode = false;

    //Reconnection: ONE policy, ONE timer. No QTimer::singleShot(1000, login)
    //anywhere else in the application any more.
    ReconnectPolicy reconnectPolicy;
    QTimer *reconnectTimer = nullptr;
    ReconnectPolicy::State lastEmittedState = ReconnectPolicy::Disconnected;
    //Since the last attempt was started, used to rate limit the "shortcut the
    //pending delay" requests coming from network discovery.
    QElapsedTimer lastAttemptTimer;

    //When to send register_push, see PushRegistration.
    PushRegistration pushRegistration;
    //Sends register_push over the current transport if pushRegistration says
    //the current token is not registered in this session yet.
    void registerPushToken();

    //Set as soon as one of onWsDisconnected()/onWsError() switched to HTTP, so
    //a socket that errors *and* then closes cannot start two HTTP logins.
    //Reset by every login()/logout() through resetTransport().
    bool wsFallbackDone = false;

    void connectWebsocket(QString h);
    void closeWebsocket();
    void connectHttp(QString h);

    //Single entry point of every connection attempt, automatic or not.
    void startConnection(QString user, QString pass, QString h, ReconnectPolicy::Trigger trigger);
    //Tears the transport down without touching the policy: shared by
    //logout(), suspend() and every new attempt. The only thing it emits, last,
    //is the failure of the event log requests still in flight (EVLOG-1).
    void resetTransport();
    //The session is over because something broke. Tears down, tells the
    //policy, notifies Application and arms the single retry timer.
    void connectionLost(const QString &reason, ReconnectPolicy::Failure kind = ReconnectPolicy::TransientFailure);
    void handleWebsocketFailure(const QString &reason);

    TlsPinStore *pins() const;
    void dropCachedConnections();
    //The decision for one handshake; true means "ignore the errors" (pin
    //match). Otherwise the attempt is marked as blocked by the certificate.
    bool checkTls(const QUrl &url, const QSslCertificate &peer, const QList<QSslError> &errors);
    TlsPinStore *pinStore = nullptr;
    //What the dialog shows, empty when nothing is waiting for the user.
    QVariantMap pendingTrust;
    //"host:port|spki" the user refused (or that can never be accepted).
    QSet<QString> tlsRefused;
    //The current attempt met an untrusted certificate: its failure is a
    //TrustFailure and the websocket must not fall back to HTTP.
    bool tlsBlocked = false;
    void handlePollFailure(const QString &reason);
    void armReconnectTimer();
    void cancelReconnectTimer();
    void emitStateChanged();
    void reconnectTimerFired();
    //Both report what was sent: false / nullptr when that transport is not
    //the one up.
    bool sendWebsocket(const QString &msg, const QJsonObject &data = QJsonObject(), const QString &client_id = QString());
    QNetworkReply *sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors = true);

    void processEventsV2(QString msg);
    void processEventsV3(QVariantMap msg);

    //Turns what CalaosEventDecoder decoded into the existing signals. This is
    //all that is left here of the event handling: the protocol itself lives in
    //CalaosEventDecoder and is unit tested without a server (T17).
    void dispatchEvents(const QList<DecodedEvent> &events);

    //Replaces the value of credential carrying query parameters (cn_user,
    //cn_pass, u, p) by "***" so URLs can be safely printed in logs.
    //Only used for logging, the URLs actually sent are left untouched.
    static QString redactUrl(const QUrl &url);
    //Same idea for the JSON payloads that are dumped in debug builds.
    static QByteArray redactJson(const QJsonObject &obj);

signals:
    void homeLoaded(const QVariantMap &home);
    void disconnected();
    void loginFailed();

    //events signals
    void eventInputChange(QString id, QString state, QString value);
    void eventOutputChange(QString id, QString state, QString value);
    void eventInputAdd(QString id);
    void eventOutputAdd(QString id);
    void eventInputDel(QString id);
    void eventOutputDel(QString id);
    void eventRoomChange(QString name, QString type, QString value);
    void eventRoomAdd(QString name, QString type);
    void eventRoomDel(QString name, QString type);
    void eventAudioChange(QString playerid);
    void eventAudioPlaylistChange(QString playerid);
    void eventAudioStatusChange(QString playerid, QString status);
    void eventAudioVolumeChange(QString playerid, double volume);
    void eventAudioStateChange(QString playerid, const QVariantMap &data);
    void eventInputStateChange(const QVariantMap &data);
    void eventOutputStateChange(const QVariantMap &data);
    void eventScenarioNew();
    void eventScenarioDel();
    void eventScenarioChange();
    void cameraPictureDownloaded(const QString &camid, const QByteArray &data);
    void cameraPictureFailed(const QString &camid);
    void eventTouchscreenCamera(QString camid);
    //End of an event log request, see requestEventLogPage(). ok is true when
    //the server sent what was asked for (a page carries "events"); the map
    //is the server's answer either way ({"error": ...} on a server error),
    //empty when the transport failed.
    void eventLogPageFinished(const QString &requestId, bool ok, const QVariantMap &page);
    void eventLogEventFinished(const QString &requestId, bool ok, const QVariantMap &event);
    void audioCoverDownloaded(const QString &camid, const QByteArray &data);
    void changeCredsSuccess(QString uname, QString pass);
    void changeCredsFailed();
    void eventIoStatusChange(QString id, QVariantMap statusData);
    void eventDeviceChange(QString id, QVariantMap device);
    void eventCommandRefused(QString id);
    void eventCommandAnswered(QString id, int seq, bool ok);

    //Lifecycle changes, the value is a ReconnectPolicy::State. Application
    //derives its applicationStatus from this instead of tracking it in
    //parallel.
    void connectionStateChanged(int state);

    //See answerTlsTrust(). info: host (host:port), fingerprint (SPKI
    //SHA-256), certFingerprint (whole certificate SHA-256), subject, issuer,
    //expiry, keyChanged, previousFingerprint, suspicious (DNS name, or a
    //valid chain for another name: no reassuring wording), caValid (Qt
    //validated the certificate: only asked when it contradicts a pin).
    void tlsTrustRequested(const QVariantMap &info);
    //The pending question is void (logout): close the dialog.
    void tlsTrustCanceled();
    //The user refused the certificate of host.
    void tlsTrustRejected(const QString &host);
    //A certificate no pin can make acceptable (revoked, forged signature).
    //weakKey: RSA under 2048 bits, the box needs a new certificate.
    void tlsCertificateRefused(const QString &host, const QString &reason, bool weakKey);

public slots:
    void login(QString user, QString pass, QString host);
    void logout();
    //IOConnection, with the default arguments the rest of the app relies on.
    void sendCommand(QString id, QString value, QString type = QString(), QString action = QString()) override;
    void sendNumberedCommand(QString id, QString value, QString type, QString action, int seq) override;
    void queryState(QStringList inputs, QStringList outputs, QStringList audio_players);
    void getCameraPicture(const QString &camid, QString urlSuffix = QString());
    void getAudioCover(const QString &playerid);

private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void sslErrorsWebsocket(const QList<QSslError> &errors);
    void encrypted(QNetworkReply *reply);
    void loginFinished(QNetworkReply *reply);

    void requestFinished();
    void requestCamFinished(QNetworkReply *reqReply, const QString &camid);
    void requestError(QNetworkReply::NetworkError code);
    void requestAudioCoverFinished(QNetworkReply *reqReply, const QString &camid);

    void startJsonPolling();

    //Websocket slots
    void onWsConnected();
    void onWsDisconnected();
    void onWsError();
    void onWsTextMessageReceived(const QString &message);
};

#endif // CALAOSCONNECTION_H
