#ifndef CALAOSCONNECTION_H
#define CALAOSCONNECTION_H

#include <QtCore>
#include <QtNetwork>
#include <QRandomGenerator>
#include <QWebSocket>

#include "CalaosEventDecoder.h"

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
    };

    enum Trigger
    {
        UserRequest, //explicit login (login screen, app resume): resets backoff
        AutoRetry,   //backoff timer, server discovery, network coming back
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
    bool canBeginAttempt(Trigger t) const
    {
        if (m_state == Connecting || m_state == Connected)
            return false;
        if (t == AutoRetry && m_givenUp)
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

    //Explicit logout: forget everything, including that a session was wanted.
    void stop()
    {
        m_state = Disconnected;
        m_failures = 0;
        m_authFailures = 0;
        m_pollFailures = 0;
        m_givenUp = false;
        m_pendingDelayMs = 0;
        m_wantsConnection = false;
    }

    //The link is known to be down (airplane mode, cable unplugged): drop the
    //session but remember that one is wanted, so canResume() says yes when the
    //link comes back.
    void suspend()
    {
        m_state = Disconnected;
        m_failures = 0;
        m_pollFailures = 0;
        m_pendingDelayMs = 0;
    }

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
    bool m_jitterEnabled = true;
};

class CalaosConnection : public QObject
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

    void updateHttpApiV2(bool en) { isV2HttpApi = en; }
    bool isHttpApiV2() { return isV2HttpApi; }
    bool isWebsocket() { return wsocket && constate == ConStateWebsocket; }
    bool isHttp() { return constate == ConStateHttp; }

    void sendJson(QString action, QJsonObject &jsonData);

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

    //Network discovery found a server: try again *now* instead of waiting for
    //the pending backoff delay. Never stacks a second timer or a second login,
    //the guard in ReconnectPolicy refuses the call when an attempt is already
    //in flight or when we gave up on the credentials.
    void requestImmediateReconnect(QString user, QString pass, QString host);

    //The link is known to be down: drop the session but remember it is wanted.
    void suspend();
    //The link is back: reconnect immediately if a session was wanted.
    void resume(QString user, QString pass, QString host);

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
    QNetworkReply *pollReply;

    QWebSocket *wsocket = nullptr;

    bool isV2HttpApi = true;

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

    //Set as soon as one of onWsDisconnected()/onWsError() switched to HTTP, so
    //a socket that errors *and* then closes cannot start two HTTP logins.
    //Reset by every login()/logout() through resetTransport().
    bool wsFallbackDone = false;

    void connectWebsocket(QString h);
    void closeWebsocket();
    void connectHttp(QString h);

    //Single entry point of every connection attempt, automatic or not.
    void startConnection(QString user, QString pass, QString h, ReconnectPolicy::Trigger trigger);
    //Tears the transport down without touching the policy and without
    //emitting anything: shared by logout(), suspend() and every new attempt.
    void resetTransport();
    //The session is over because something broke. Tears down, tells the
    //policy, notifies Application and arms the single retry timer.
    void connectionLost(const QString &reason, ReconnectPolicy::Failure kind = ReconnectPolicy::TransientFailure);
    void handleWebsocketFailure(const QString &reason);
    void handlePollFailure(const QString &reason);
    void armReconnectTimer();
    void cancelReconnectTimer();
    void emitStateChanged();
    void reconnectTimerFired();
    void sendWebsocket(const QString &msg, const QJsonObject &data = QJsonObject(), const QString &client_id = QString());
    void sendHttp(const QString &msg, QJsonObject &data, bool ignoreErrors = true);

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
    void logEventLoaded(const QVariantMap &data);
    void audioCoverDownloaded(const QString &camid, const QByteArray &data);
    void changeCredsSuccess(QString uname, QString pass);
    void changeCredsFailed();
    void eventIoStatusChange(QString id, QVariantMap statusData);

    //Lifecycle changes, the value is a ReconnectPolicy::State. Application
    //derives its applicationStatus from this instead of tracking it in
    //parallel.
    void connectionStateChanged(int state);

public slots:
    void login(QString user, QString pass, QString host);
    void logout();
    void sendCommand(QString id, QString value, QString type = QString(), QString action = QString());
    void queryState(QStringList inputs, QStringList outputs, QStringList audio_players);
    void getCameraPicture(const QString &camid, QString urlSuffix = QString());
    void getAudioCover(const QString &playerid);

private slots:
    void sslErrors(QNetworkReply *reply, const QList<QSslError> &);
    void sslErrorsWebsocket(const QList<QSslError> &errors);
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
