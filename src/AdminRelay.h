#ifndef ADMINRELAY_H
#define ADMINRELAY_H

#include <functional>
#include <QObject>
#include <QHash>
#include <QDeadlineTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include "AdminTransport.h"

class QTimer;
class CalaosConnection;

/*
 * Client side of calaos_server's admin messages (calaos_base
 * docs/08_http_api.md, "Administration"), over the household WebSocket:
 * admin_unlock, admin {action, params}, and the admin_event pushes.
 *
 * Holds the admin session in memory only: it is never persisted, it dies with
 * the socket, at logout, after its absolute lifetime (15 min at most) and when
 * a new certificate key is trusted. Requests are correlated by msg_id and
 * time out like the webapp's (5 s, 125 s for update_check). Every reply moves
 * the gate, named after the webapp store's: the screen the box allows.
 */
class AdminRelay: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString gate READ gate NOTIFY gateChanged)
    Q_PROPERTY(bool isUnlocked READ isUnlocked NOTIFY sessionChanged)

public:
    //Gates. "idle": nothing asked yet; "probing": the first update_status is out.
    static inline const QString GateIdle = QStringLiteral("idle");
    static inline const QString GateProbing = QStringLiteral("probing");
    static inline const QString GateLocked = QStringLiteral("locked");
    static inline const QString GateUnlocked = QStringLiteral("unlocked");
    static inline const QString GateHttpsRequired = QStringLiteral("https_required");
    static inline const QString GateAdminNotSet = QStringLiteral("admin_not_set");
    static inline const QString GateUnavailable = QStringLiteral("unavailable");
    static inline const QString GateTooOld = QStringLiteral("too_old");
    static inline const QString GateUnreachable = QStringLiteral("unreachable");
    static inline const QString GateOldServer = QStringLiteral("old_server");
    //logged in over the HTTP fallback: no admin messages there
    static inline const QString GateWebsocketRequired = QStringLiteral("websocket_required");

    //Codes produced by the client itself, beside the server's vocabulary.
    static inline const QString ErrTimeout = QStringLiteral("timeout");
    static inline const QString ErrDisconnected = QStringLiteral("disconnected");
    static inline const QString ErrWebsocketRequired = QStringLiteral("websocket_required");
    static inline const QString ErrTlsPending = QStringLiteral("tls_pending");
    static inline const QString ErrUnknown = QStringLiteral("unknown");
    //unlock refused: no admin answer yet on this socket
    static inline const QString ErrNotReady = QStringLiteral("not_ready");
    //the session ended while a request made with it was out
    static inline const QString ErrSessionLost = QStringLiteral("session_lost");

    static constexpr int ReplyTimeoutMs = 5000;
    static constexpr int UpdateCheckTimeoutMs = 125000;
    static constexpr int MaxSessionSeconds = 900;

    struct Error
    {
        QString code;
        QString msg;         //the server's English sentence, display fallback only
        int retryAfter = -1; //seconds, with rate_limited
        QString jobId;       //with upgrade_in_progress
    };

    struct Outcome
    {
        bool ok = false;
        QJsonValue result; //"result" of an admin reply, typed JSON
        Error error;
    };

    using Callback = std::function<void(const Outcome &)>;

    explicit AdminRelay(AdminTransport *transport, QObject *parent = nullptr);
    ~AdminRelay() override;

    //Wires the household connection: replies, events, the end of the socket
    //and the certificate answers.
    void bindConnection(CalaosConnection *con);

    //admin_unlock. Refused (nothing sent) while a certificate question waits,
    //over the HTTP fallback, before an admin answer on the current socket,
    //and on a socket that is not wss (https_required).
    void unlock(const QString &password, Callback callback = Callback());
    //admin {action, params}, with the session when one is held (update_status
    //then gets the full view, the household view otherwise).
    void request(const QString &action, const QJsonObject &params, Callback callback);

    QString gate() const { return m_gate; }
    bool isUnlocked() const { return !m_session.isEmpty(); }
    //Seconds left, 0 without a session.
    int expiresIn() const;
    QString session() const { return m_session; }
    //An admin reply came on this socket: silence afterwards is a stall.
    bool hasSpoken() const { return m_spoken; }

    //While an upgrade is followed a silent box is busy (dpkg), not old.
    void setUpgrading(bool upgrading) { m_upgrading = upgrading; }
    //Drops the session (user locked the screen, session invalidated).
    void lock();

    //Tests shorten the delays.
    void setTimeouts(int replyMs, int updateCheckMs);

public slots:
    void handleReply(const QString &msg, const QString &msgId, const QJsonObject &data);
    void handleEvent(const QJsonObject &data);
    //The socket is gone (logout, lost link, background): the session with it.
    void handleTransportLost();
    void handleTransportUp();
    void handleTlsTrustAccepted();
    //Timers do not run in the background: the lifetime is checked again on
    //the way back.
    void checkExpiry();

signals:
    void gateChanged();
    void sessionChanged();
    //admin_event upgrade_progress: the household <Status> (no package names)
    void upgradeProgress(const QJsonObject &status);
    //admin_set / admin_reset while unlocked: someone changed the password
    void sessionInvalidated();
    //admin_reset: the box has no admin any more, probe again
    void adminReset();
    //A logged-in socket is up again
    void transportUp();

private:
    struct Pending
    {
        QString msg;
        QString action;
        Callback callback;
        bool withSession = false;
        QTimer *timer = nullptr;
    };

    QString send(const QString &msg, const QString &action, const QJsonObject &data,
                 int timeoutMs, bool withSession, Callback callback);
    void finish(const QString &msgId, const Outcome &outcome);
    void failLater(const QString &code, Callback callback);
    void applyOutcome(const QString &msg, const QString &action, Outcome &outcome);
    void setGate(const QString &gate);
    void setSession(const QString &session, int expiresInSeconds);
    void forgetSession();

    AdminTransport *m_transport = nullptr;
    QString m_gate = GateIdle;
    QString m_session;
    QDeadlineTimer m_deadline;
    //Wall clock too: the monotonic clock may stop while the phone sleeps.
    qint64 m_wallExpiry = 0;
    QTimer *m_sessionTimer = nullptr;
    QHash<QString, Pending> m_pending;
    quint64 m_nextId = 0;
    bool m_spoken = false;
    //an admin answer came on the current socket: unlock may go out
    bool m_probeAnswered = false;
    bool m_upgrading = false;
    int m_replyTimeoutMs = ReplyTimeoutMs;
    int m_updateCheckTimeoutMs = UpdateCheckTimeoutMs;
};

#endif // ADMINRELAY_H
