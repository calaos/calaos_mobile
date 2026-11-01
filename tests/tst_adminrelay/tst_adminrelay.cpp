/*
 * Box updates from the phone (CM1).
 *
 * AdminRelay is the client of calaos_server's admin messages
 * (calaos_base docs/08_http_api.md, "Administration"): admin_unlock and
 * admin {action, params} correlated by msg_id, 5 s / 125 s timeouts, the
 * gates of the webapp store, and an admin session that lives in memory only
 * and dies with the socket, at expiry, on admin_set/admin_reset and when a
 * new certificate key is trusted.
 *
 *  - against a fake transport: every reply shape, correlation, timeouts,
 *    gates, the session lifecycle, nothing secret in the logs;
 *  - against a real CalaosConnection and a fake calaos_server websocket on
 *    127.0.0.1: what goes on the wire, and the session dying with it;
 *  - AdminRelayBackend + AdminUpdates: relayed <Status>/<Job> mapped onto
 *    the shared update models, package names only with a session.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QSslKey>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include "AdminBiometrics.h"
#include "AdminRelay.h"
#include "AdminRelayBackend.h"
#include "AdminUpdates.h"
#include "HardwareUtils.h"
#include "SecretWipe.h"
#include "CalaosConnection.h"
#include "TlsPinning.h"
#include "ResumePolicy.h"

static const QString TestUser = QStringLiteral("user");
static const QString TestPass = QStringLiteral("pass");
static const QString AdminPass = QStringLiteral("correct horse battery");
static const QString Session = QStringLiteral("9b1f00112233445566778899aabbccddeeff00112233445566778899aabbcce4");

//Everything the code under test logs, to check no secret ends up there.
static QStringList capturedLogs;
static QtMessageHandler previousHandler = nullptr;
static void captureHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    capturedLogs << msg;
    if (previousHandler)
        previousHandler(type, ctx, msg);
}

struct Frame
{
    QString msg;
    QJsonObject data;
    QString msgId;
};

class FakeTransport: public AdminTransport
{
public:
    bool up = true;
    bool trustPending = false;
    bool secure = true;
    bool httpFallback = false;
    QString host = QStringLiteral("box-a.example:443");
    QList<Frame> frames;

    bool sendAdminMessage(const QString &msg, const QJsonObject &data, const QString &msgId) override
    {
        if (!up)
            return false;
        frames << Frame{ msg, data, msgId };
        return true;
    }

    bool hasPendingTlsTrust() const override { return trustPending; }
    bool isSecure() const override { return up && secure; }
    bool isHttpFallback() const override { return httpFallback; }
    QString adminHostKey() const override { return up ? host : QString(); }

    //The last frame sent for that admin action (or message).
    int last(const QString &actionOrMsg) const
    {
        for (int i = frames.size() - 1; i >= 0; i--)
        {
            if (frames.at(i).data.value("action").toString() == actionOrMsg ||
                (frames.at(i).msg == actionOrMsg && actionOrMsg != "admin"))
                return i;
        }
        return -1;
    }

    int count(const QString &action) const
    {
        int n = 0;
        for (const Frame &f: frames)
            n += f.data.value("action").toString() == action ? 1 : 0;
        return n;
    }
};

static QJsonObject ok(const QJsonValue &result = QJsonValue())
{
    QJsonObject o{{ "success", "true" }};
    if (!result.isUndefined())
        o["result"] = result;
    return o;
}

static QJsonObject err(const QString &code, const QJsonObject &extra = QJsonObject())
{
    QJsonObject o{{ "success", "false" }, { "error", code }, { "msg", "server text" }};
    for (auto it = extra.begin(); it != extra.end(); ++it)
        o[it.key()] = it.value();
    return o;
}

static QJsonObject unlockOk(const QString &expires = QStringLiteral("900"))
{
    return QJsonObject{{ "success", "true" }, { "admin_session", Session }, { "expires_in", expires }};
}

static QJsonObject job(const QString &id, const QString &status, bool reboot = false)
{
    QJsonObject j{{ "id", id }, { "kind", "upgrade" }, { "status", status },
                  { "reboot_required", reboot }, { "daemon_restart", false },
                  { "started_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate) }};
    if (status != "running")
        j["finished_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return j;
}

static QJsonObject progress(const QString &status, int percent, int index, int count,
                            const QStringList &names = QStringList())
{
    QJsonArray pkgs;
    for (int i = 0; i < count; i++)
    {
        QJsonObject p{{ "state", i < index - 1 ? "done" : "installing" }, { "percent", i < index - 1 ? 100 : 30 }};
        if (!names.isEmpty())
            p["name"] = names.value(i);
        pkgs.append(p);
    }
    QJsonObject o{{ "status", status }, { "total_percent", percent }, { "pkg_index", index },
                  { "pkg_count", count }, { "packages", pkgs }};
    if (!names.isEmpty() && index >= 1)
        o["current_pkg"] = names.value(index - 1);
    return o;
}

/*
 * calaos_server's websocket reduced to login and the admin messages:
 * admin_unlock checks AdminPass, admin answers update_status, everything
 * else per mode. Silent: an old calaos_server, which ignores unknown
 * messages. HttpsRequired: the box reached over plain 5454.
 */
class FakeAdminWs: public QObject
{
    Q_OBJECT

public:
    enum Mode { Normal, Silent, HttpsRequired };
    Mode mode = Normal;
    QList<QJsonObject> adminFrames; //every admin_* / admin frame received

    bool start(quint16 port = 0)
    {
        if (!server->listen(QHostAddress::LocalHost, port))
            return false;
        m_port = server->serverPort();
        return true;
    }

    void stop()
    {
        const QList<QWebSocket *> socks = clients;
        clients.clear();
        for (QWebSocket *c: socks)
            c->abort();
        server->close();
    }

    quint16 port() const { return m_port; }
    QString url() const
    {
        return QStringLiteral("%1://127.0.0.1:%2/api").arg(m_secure ? "wss" : "ws").arg(m_port);
    }

    void pushEvent(const QJsonObject &data)
    {
        for (QWebSocket *c: std::as_const(clients))
            c->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{ "msg", "admin_event" },
                                                                         { "data", data }}).toJson(QJsonDocument::Compact)));
    }

    //tls: serve wss with that certificate, plain ws otherwise
    explicit FakeAdminWs(const QSslConfiguration *tls = nullptr):
        m_secure(tls != nullptr)
    {
        server = new QWebSocketServer(QStringLiteral("fake calaos_server"),
                                      tls ? QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode,
                                      this);
        if (tls)
            server->setSslConfiguration(*tls);
        connect(server, &QWebSocketServer::newConnection, this, [this]()
        {
            QWebSocket *c = server->nextPendingConnection();
            clients << c;
            connect(c, &QWebSocket::disconnected, this, [this, c]()
            {
                clients.removeAll(c);
                c->deleteLater();
            });
            connect(c, &QWebSocket::textMessageReceived, this, [this, c](const QString &m)
            {
                onMessage(c, QJsonDocument::fromJson(m.toUtf8()).object());
            });
        });
    }

    ~FakeAdminWs() override { stop(); }

private:
    void onMessage(QWebSocket *c, const QJsonObject &j)
    {
        const QString msg = j["msg"].toString();
        auto reply = [c, &j, msg](const QJsonObject &data)
        {
            QJsonObject o{{ "msg", msg }, { "data", data }};
            if (j.contains("msg_id"))
                o["msg_id"] = j["msg_id"];
            c->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
        };

        if (msg == "login")
        {
            const QJsonObject d = j["data"].toObject();
            const bool okLogin = d["cn_user"] == TestUser && d["cn_pass"] == TestPass;
            reply(QJsonObject{{ "success", okLogin ? "true" : "false" }});
            return;
        }
        if (msg == "get_home")
        {
            reply(QJsonObject{{ "home", QJsonArray() }});
            return;
        }
        if (msg != "admin_unlock" && msg != "admin")
            return;

        adminFrames << j;
        if (mode == Silent)
            return;
        if (mode == HttpsRequired)
        {
            reply(err("https_required"));
            return;
        }

        const QJsonObject d = j["data"].toObject();
        if (msg == "admin_unlock")
        {
            reply(d["admin_pass"] == AdminPass ? unlockOk() : err("bad_password"));
            return;
        }
        if (d["action"] == "update_status")
        {
            const bool full = d["admin_session"] == Session;
            reply(ok(QJsonObject{{ "progress", progress("idle", 0, 0, 0) },
                                 { "job", full ? QJsonValue(job("j1", "done")) : QJsonValue(QJsonValue::Null) }}));
            return;
        }
        reply(d.contains("admin_session") ? ok(QJsonObject()) : err("bad_session"));
    }

    QWebSocketServer *server = nullptr;
    QList<QWebSocket *> clients;
    quint16 m_port = 0;
    bool m_secure = false;
};

//The platform's protected storage, without a prompt: the tests answer.
class FakeBiometricHw: public HardwareUtils
{
public:
    FakeBiometricHw(): HardwareUtils(nullptr) {}
    bool enrolled = true;
    //The store waits for answerStore(), as behind an Android prompt.
    bool deferStore = false;
    int canceledStores = 0;
    QHash<QString, QString> vault;
    QStringList reads;
    QString deferredKey, deferredSecret;
    bool hasBiometrics() const override { return enrolled; }
    void biometricStore(const QString &key, const QString &secret, const QString &) override
    {
        deferredKey = key;
        deferredSecret = secret;
        if (!deferStore)
            answerStore(BiometricOk);
    }
    void biometricCancelStore() override { canceledStores++; }
    void biometricRead(const QString &key, const QString &) override { reads << key; }
    void biometricRemove(const QString &key) override { vault.remove(key); }
    void biometricRemoveAll() override { vault.clear(); }
    void answerRead(int result = BiometricOk)
    {
        const QString key = reads.last();
        emitBiometricReadDone(key, result, result == BiometricOk ? vault.value(key) : QString());
    }
    void answerStore(int result)
    {
        if (result == BiometricOk)
            vault[deferredKey] = deferredSecret;
        emitBiometricStoreDone(deferredKey, result);
    }
};

//A box whose updates page is open and locked.
struct BiometricFixture
{
    FakeTransport t;
    AdminRelay relay{ &t };
    AdminRelayBackend backend{ &relay };
    AdminUpdates screen{ &backend, &relay };
    FakeBiometricHw hw;
    AdminBiometrics bio{ &hw, &relay, &screen, &t };

    BiometricFixture()
    {
        QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
        settings.remove(QStringLiteral("admin_biometrics"));
        settings.sync();
    }

    void openLocked()
    {
        screen.activate();
        answerProbe();
    }

    void answerProbe()
    {
        relay.handleReply("admin", t.frames.last().msgId,
                          ok(QJsonObject{{ "progress", QJsonObject() }, { "job", QJsonValue::Null }}));
        QTest::qWait(1);
    }

    void answerUnlock(const QJsonObject &answer = unlockOk())
    {
        relay.handleReply("admin_unlock", t.frames.last().msgId, answer);
    }

    //The password stored, the page left, the socket gone.
    void store()
    {
        openLocked();
        bio.unlockAndKeep(AdminPass);
        answerUnlock();
        relay.handleTransportLost();
        screen.deactivate();
        QTest::qWait(1);
        QVERIFY(hw.reads.isEmpty()); //no prompt for a socket going away
        t.frames.clear();
    }

    //Back on the page with what store() left, socket up.
    void reopen()
    {
        relay.handleTransportUp();
        openLocked();
    }

    //Read done and held, the socket gone.
    void holdWithoutSocket()
    {
        store();
        reopen();
        relay.handleTransportLost();
        t.up = false;
        hw.answerRead();
    }

    void socketBack()
    {
        t.up = true;
        relay.handleTransportUp();
        screen.retry();
        answerProbe();
    }

    int unlocks() const
    {
        int n = 0;
        for (const Frame &f: t.frames)
            n += f.msg == "admin_unlock" ? 1 : 0;
        return n;
    }
};

class TstAdminRelay: public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    QSslConfiguration tls;
    bool tlsReady = false;

    //Self-signed certificate for the wss fake server, as tst_tlspinning does.
    bool makeServerConfig()
    {
        const QString key = dir.filePath(QStringLiteral("key.pem"));
        const QString crt = dir.filePath(QStringLiteral("crt.pem"));
        QProcess openssl;
        openssl.start(QStringLiteral("openssl"),
                      { "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=calaos.local", "-keyout", key, "-out", crt });
        if (!openssl.waitForFinished(30000) || openssl.exitStatus() != QProcess::NormalExit ||
            openssl.exitCode() != 0)
            return false;
        QFile kf(key), cf(crt);
        if (!kf.open(QIODevice::ReadOnly) || !cf.open(QIODevice::ReadOnly))
            return false;
        const QSslKey sslKey(kf.readAll(), QSsl::Rsa, QSsl::Pem);
        const QList<QSslCertificate> certs = QSslCertificate::fromData(cf.readAll(), QSsl::Pem);
        if (sslKey.isNull() || certs.isEmpty())
            return false;
        tls = QSslConfiguration::defaultConfiguration();
        tls.setLocalCertificate(certs.first());
        tls.setPrivateKey(sslKey);
        tls.setPeerVerifyMode(QSslSocket::VerifyNone);
        return true;
    }

    //An admin answer on the current socket: unlock may go out.
    void probeRelay(FakeTransport &t, AdminRelay &relay)
    {
        relay.request("update_status", QJsonObject(), {});
        relay.handleReply("admin", t.frames.last().msgId,
                          ok(QJsonObject{{ "progress", QJsonObject() }, { "job", QJsonValue::Null }}));
    }

    //Unlocked relay over t, the session in hand.
    void unlockRelay(FakeTransport &t, AdminRelay &relay)
    {
        probeRelay(t, relay);
        const int before = t.frames.size();
        relay.unlock(AdminPass);
        QCOMPARE(t.frames.size(), before + 1);
        relay.handleReply("admin_unlock", t.frames.last().msgId, unlockOk());
        QVERIFY(relay.isUnlocked());
    }

private slots:
    void initTestCase()
    {
        previousHandler = qInstallMessageHandler(captureHandler);
        QVERIFY(dir.isValid());
        if (QSslSocket::availableBackends().contains(QStringLiteral("openssl")))
            QSslSocket::setActiveBackend(QStringLiteral("openssl"));
        tlsReady = QSslSocket::supportsSsl() && makeServerConfig();
        if (!tlsReady && qEnvironmentVariableIsSet("CI"))
            qFatal("No TLS backend or no openssl CLI to make a test certificate");
    }

    void cleanupTestCase()
    {
        qInstallMessageHandler(previousHandler);
        //SR-21: neither the admin password nor the session reached a log
        for (const QString &line: std::as_const(capturedLogs))
        {
            QVERIFY2(!line.contains(AdminPass), qPrintable(line));
            QVERIFY2(!line.contains(Session), qPrintable(line));
        }
    }

    void unlockSuccessStoresSession();
    void unlockBadPasswordKeepsLocked();
    void gatesFromErrorCodes_data();
    void gatesFromErrorCodes();
    void silentServerIsOldServer();
    void timeoutAfterAnAnswerIsNotOldServer();
    void timeoutWhileUpgradingIsNotOldServer();
    void repliesCorrelatedByMsgId();
    void updateCheckHasLongTimeout();
    void noUnlockWhileTlsTrustPending();
    void trustAcceptedDropsSession();
    void transportLostDropsSessionAndFailsPending();
    void adminSetEventInvalidatesSession();
    void adminResetEventProbesAgain();
    void sessionExpires();
    void expiresInCapped();
    void badSessionDropsSessionSilently();
    void householdStatusCarriesNoSession();
    void notSentWithoutWebsocket();
    void progressEventForwarded();
    void gateIdleAfterTransportLost();
    void unlockOverPlainWsSendsNothing();
    void unlockBeforeProbeOnNewSocketSendsNothing();
    void malformedSessionRefused();
    void timeoutWhileLockedIsUnreachable();
    void sessionLossFailsSessionRequests();

    void realConnectionUnlockAndStatus();
    void realConnectionServerDropKillsSession();
    void realConnectionNoPasswordOverPlainWs();
    void realConnectionOldServer();
    void realConnectionHttpsRequired();

    void backendMapsReplies();
    void unlockReadsFullView();
    void checkMapsPackagesAndGroups();
    void upgradeAllFollowsJobToReboot();
    void householdViewHidesNames();
    void sessionLossClearsPackageList();
    void rateLimitedCountdown();
    void rebootDuringUpgradeRefused();
    void blockedGateRetries();
    void failedJobReason();
    void checkAnswerAfterSessionLossDropped();
    void sessionLossReducesToHouseholdView();
    void httpFallbackGate();
    void rateLimitedOnActionCountsDown();
    void unansweredUpgradeReadsStatus();
    void rebootGuardedDuringUpgrade();
    void oldSocketErrorClearedOnReconnect();
    void replyForAnotherMessageIgnored();
    void resumePolicyKeepsThePage();

    void biometricsStoredOnlyWhenAsked();
    void biometricsStoredOnceTheBoxAccepted();
    void biometricsNotStoredWithoutEnrolment();
    void biometricsStoreCanceledOrFailedLeavesNothing();
    void biometricsSecretNeverInSettings();
    void biometricsAskedWhenTheCardOpens();
    void biometricsCancelLeavesTheField();
    void biometricsFailedReadLeavesTheField();
    void biometricsChangedEnrolmentForgets();
    void biometricsRefusedPasswordForgets();
    void biometricsTypedRefusalKeepsTheSecret();
    void biometricsOtherRefusalKeepsTheSecret();
    void biometricsReadWaitsForTheNewSocket();
    void biometricsReadDroppedByTheTimer();
    void biometricsReadDroppedByTheClock();
    void biometricsSuspensionDropsWhatIsHeld();
    void biometricsInactiveKeepsTheRead();
    void biometricsNeverSentToAnotherBox();
    void biometricsCertificateQuestionDropsTheRead();
    void biometricsCertificateSignalsOfTheConnection();
    void biometricsForgetDuringARead();
    void biometricsForgetDuringAStore();
    void biometricsRowStaysWithoutEnrolment();
    void biometricsDisable();
    void biometricsForgetAllAndForgetBox();
    void biometricsWipeOnUserActions();
    void biometricsFirstLaunchEmptiesTheStore();
    void biometricsOutlivesTheUpdatesPage();
};

void TstAdminRelay::unlockSuccessStoresSession()
{
    FakeTransport t;
    AdminRelay relay(&t);
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
    probeRelay(t, relay);
    t.frames.clear();

    AdminRelay::Outcome got;
    relay.unlock(AdminPass, [&got](const AdminRelay::Outcome &o) { got = o; });
    QCOMPARE(t.frames.size(), 1);
    QCOMPARE(t.frames[0].msg, QStringLiteral("admin_unlock"));
    QCOMPARE(t.frames[0].data.value("admin_pass").toString(), AdminPass);
    QVERIFY(!t.frames[0].msgId.isEmpty());

    relay.handleReply("admin_unlock", t.frames[0].msgId, unlockOk());
    QVERIFY(got.ok);
    QVERIFY(relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateUnlocked);
    QVERIFY(relay.expiresIn() > 890 && relay.expiresIn() <= 900);

    //every later action carries it
    relay.request("update_check", QJsonObject(), {});
    QCOMPARE(t.frames.last().msg, QStringLiteral("admin"));
    QCOMPARE(t.frames.last().data.value("admin_session").toString(), Session);
    QCOMPARE(t.frames.last().data.value("action").toString(), QStringLiteral("update_check"));
    QVERIFY(t.frames.last().msgId != t.frames[0].msgId);
}

void TstAdminRelay::unlockBadPasswordKeepsLocked()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.request("update_status", QJsonObject(), {});
    relay.handleReply("admin", t.frames[0].msgId, ok(QJsonObject{{ "progress", QJsonObject() }}));
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);

    AdminRelay::Outcome got;
    relay.unlock("wrong", [&got](const AdminRelay::Outcome &o) { got = o; });
    relay.handleReply("admin_unlock", t.frames.last().msgId, err("bad_password"));
    QVERIFY(!got.ok);
    QCOMPARE(got.error.code, QStringLiteral("bad_password"));
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
}

void TstAdminRelay::gatesFromErrorCodes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("gate");
    QTest::newRow("https_required") << "https_required" << AdminRelay::GateHttpsRequired;
    QTest::newRow("admin_not_set") << "admin_not_set" << AdminRelay::GateAdminNotSet;
    QTest::newRow("admin_unavailable") << "admin_unavailable" << AdminRelay::GateUnavailable;
    QTest::newRow("daemon_too_old") << "daemon_too_old" << AdminRelay::GateTooOld;
    QTest::newRow("daemon_unreachable") << "daemon_unreachable" << AdminRelay::GateUnreachable;
    QTest::newRow("internal on the probe") << "internal" << AdminRelay::GateUnreachable;
}

void TstAdminRelay::gatesFromErrorCodes()
{
    QFETCH(QString, code);
    QFETCH(QString, gate);
    FakeTransport t;
    AdminRelay relay(&t);
    relay.request("update_status", QJsonObject(), {});
    QCOMPARE(relay.gate(), AdminRelay::GateProbing);
    relay.handleReply("admin", t.frames[0].msgId, err(code));
    QCOMPARE(relay.gate(), gate);

    //an answer clears a blocked gate
    relay.request("update_status", QJsonObject(), {});
    relay.handleReply("admin", t.frames.last().msgId, ok(QJsonObject()));
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
}

void TstAdminRelay::silentServerIsOldServer()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    AdminRelay::Outcome got;
    bool called = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { got = o; called = true; });
    QTRY_VERIFY(called);
    QCOMPARE(got.error.code, AdminRelay::ErrTimeout);
    QCOMPARE(relay.gate(), AdminRelay::GateOldServer);
    //and the password is not even asked for: nothing answered on this socket
    const int sent = t.frames.size();
    called = false;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { got = o; called = true; });
    QTRY_VERIFY(called);
    QCOMPARE(got.error.code, AdminRelay::ErrNotReady);
    QCOMPARE(t.frames.size(), sent);
}

void TstAdminRelay::timeoutAfterAnAnswerIsNotOldServer()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    unlockRelay(t, relay);

    bool called = false;
    AdminRelay::Outcome got;
    relay.request("system_info", QJsonObject(), [&](const AdminRelay::Outcome &o) { got = o; called = true; });
    QTRY_VERIFY(called);
    QCOMPARE(got.error.code, AdminRelay::ErrTimeout);
    QCOMPARE(relay.gate(), AdminRelay::GateUnlocked);
    QVERIFY(relay.isUnlocked());
}

void TstAdminRelay::timeoutWhileUpgradingIsNotOldServer()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    relay.setUpgrading(true);
    bool called = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &) { called = true; });
    QTRY_VERIFY(called);
    QCOMPARE(relay.gate(), AdminRelay::GateUnreachable);
}

void TstAdminRelay::repliesCorrelatedByMsgId()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(150, 300);
    unlockRelay(t, relay);

    QString first, second;
    relay.request("update_available", QJsonObject(), [&](const AdminRelay::Outcome &o)
    {
        first = o.result.toObject().value("tag").toString();
    });
    relay.request("system_info", QJsonObject(), [&](const AdminRelay::Outcome &o)
    {
        second = o.result.toObject().value("tag").toString();
    });
    const Frame a = t.frames[t.frames.size() - 2];
    const Frame b = t.frames.last();
    QVERIFY(a.msgId != b.msgId);

    //an unknown id is dropped, answers out of order go where they belong
    relay.handleReply("admin", "admin-999", ok(QJsonObject{{ "tag", "stray" }}));
    relay.handleReply("admin", b.msgId, ok(QJsonObject{{ "tag", "b" }}));
    relay.handleReply("admin", a.msgId, ok(QJsonObject{{ "tag", "a" }}));
    QCOMPARE(first, QStringLiteral("a"));
    QCOMPARE(second, QStringLiteral("b"));

    //a late answer to a request that timed out is ignored
    int calls = 0;
    relay.request("system_info", QJsonObject(), [&](const AdminRelay::Outcome &) { calls++; });
    const QString lateId = t.frames.last().msgId;
    QTRY_COMPARE(calls, 1);
    relay.handleReply("admin", lateId, ok(QJsonObject()));
    QCOMPARE(calls, 1);
}

void TstAdminRelay::updateCheckHasLongTimeout()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 600);
    unlockRelay(t, relay);

    bool checkDone = false, infoDone = false;
    relay.request("update_check", QJsonObject(), [&](const AdminRelay::Outcome &) { checkDone = true; });
    relay.request("system_info", QJsonObject(), [&](const AdminRelay::Outcome &) { infoDone = true; });
    QTRY_VERIFY(infoDone);
    QVERIFY(!checkDone);
    QTRY_VERIFY(checkDone);
    QCOMPARE(AdminRelay::UpdateCheckTimeoutMs, 125000);
    QCOMPARE(AdminRelay::ReplyTimeoutMs, 5000);
}

void TstAdminRelay::noUnlockWhileTlsTrustPending()
{
    FakeTransport t;
    t.trustPending = true;
    AdminRelay relay(&t);
    AdminRelay::Outcome got;
    bool called = false;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { got = o; called = true; });
    //refused without sending, and asynchronously
    QVERIFY(!called);
    QVERIFY(t.frames.isEmpty());
    QTRY_VERIFY(called);
    QCOMPARE(got.error.code, AdminRelay::ErrTlsPending);
}

void TstAdminRelay::trustAcceptedDropsSession()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QSignalSpy session(&relay, &AdminRelay::sessionChanged);
    relay.handleTlsTrustAccepted();
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
    QCOMPARE(session.count(), 1);
}

void TstAdminRelay::transportLostDropsSessionAndFailsPending()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QString code;
    relay.request("update_check", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    relay.handleTransportLost();
    QCOMPARE(code, AdminRelay::ErrDisconnected);
    QVERIFY(!relay.isUnlocked());
    QVERIFY(!relay.hasSpoken());
    QCOMPARE(relay.session(), QString());
}

void TstAdminRelay::adminSetEventInvalidatesSession()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QSignalSpy invalidated(&relay, &AdminRelay::sessionInvalidated);
    relay.handleEvent(QJsonObject{{ "type", "admin_set" }, { "ts", "2026-10-22T09:14:00Z" }});
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(invalidated.count(), 1);
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
}

void TstAdminRelay::adminResetEventProbesAgain()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QSignalSpy reset(&relay, &AdminRelay::adminReset);
    relay.handleEvent(QJsonObject{{ "type", "admin_reset" }});
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(reset.count(), 1);
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
}

void TstAdminRelay::sessionExpires()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    relay.unlock(AdminPass);
    relay.handleReply("admin_unlock", t.frames.last().msgId, unlockOk("2"));
    QVERIFY(relay.isUnlocked());
    QVERIFY(relay.expiresIn() <= 2);
    //locked a second early
    QTRY_VERIFY_WITH_TIMEOUT(!relay.isUnlocked(), 3000);
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
    //back from the background past the expiry: nothing left either
    relay.checkExpiry();
    QVERIFY(!relay.isUnlocked());
}

void TstAdminRelay::expiresInCapped()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    relay.unlock(AdminPass);
    relay.handleReply("admin_unlock", t.frames.last().msgId, unlockOk("100000"));
    QVERIFY(relay.expiresIn() <= AdminRelay::MaxSessionSeconds);
}

void TstAdminRelay::badSessionDropsSessionSilently()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QString code;
    relay.request("update_check", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    relay.handleReply("admin", t.frames.last().msgId, err("bad_session"));
    QCOMPARE(code, QStringLiteral("bad_session"));
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
}

void TstAdminRelay::householdStatusCarriesNoSession()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.request("update_status", QJsonObject(), {});
    QVERIFY(!t.frames.last().data.contains("admin_session"));
    QVERIFY(!t.frames.last().data.contains("params"));
    relay.request("update_upgrade", QJsonObject{{ "package", "calaos-server" }}, {});
    QCOMPARE(t.frames.last().data["params"].toObject()["package"].toString(), QStringLiteral("calaos-server"));
}

void TstAdminRelay::notSentWithoutWebsocket()
{
    FakeTransport t;
    t.up = false;
    AdminRelay relay(&t);
    QString code;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, AdminRelay::ErrDisconnected);
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);

    //logged in over the HTTP fallback: a screen of its own, not "checking"
    t.httpFallback = true;
    code.clear();
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, AdminRelay::ErrWebsocketRequired);
    QCOMPARE(relay.gate(), AdminRelay::GateWebsocketRequired);
    QVERIFY(t.frames.isEmpty());
}

void TstAdminRelay::progressEventForwarded()
{
    FakeTransport t;
    AdminRelay relay(&t);
    QSignalSpy spy(&relay, &AdminRelay::upgradeProgress);
    relay.handleEvent(QJsonObject{{ "type", "upgrade_progress" }, { "ts", "x" },
                                  { "data", QJsonObject{{ "status", "upgrading" }, { "total_percent", 42 }} }});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toJsonObject()["total_percent"].toInt(), 42);
    relay.handleEvent(QJsonObject{{ "type", "push_code" }});
    QCOMPARE(spy.count(), 1);
}

//---------------------------------------------------------- real connection

void TstAdminRelay::realConnectionUnlockAndStatus()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    FakeAdminWs ws(&tls);
    QVERIFY(ws.start());
    CalaosConnection con;
    TlsPinStore pins(dir.filePath(QStringLiteral("pins_unlock.ini")));
    pins.setPin(TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws.port()),
                TlsPinning::spkiSha256(tls.localCertificate()));
    con.setPinStore(&pins);
    AdminRelay relay(&con);
    relay.bindConnection(&con);

    QSignalSpy up(&relay, &AdminRelay::transportUp);
    con.login(TestUser, TestPass, ws.url());
    QTRY_VERIFY(up.count() >= 1);
    QVERIFY(con.isSecure());

    //no password before the box has answered on this socket
    QString code;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, AdminRelay::ErrNotReady);
    QVERIFY(ws.adminFrames.isEmpty());

    //household view first: no session on the wire
    bool statusDone = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o)
    {
        QVERIFY(o.ok);
        QVERIFY(o.result.toObject()["job"].isNull());
        statusDone = true;
    });
    QTRY_VERIFY(statusDone);
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
    QVERIFY(!ws.adminFrames.last()["data"].toObject().contains("admin_session"));
    QVERIFY(ws.adminFrames.last().contains("msg_id"));

    code.clear();
    relay.unlock("wrong password", [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, QStringLiteral("bad_password"));
    QVERIFY(!relay.isUnlocked());

    bool unlocked = false;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { unlocked = o.ok; });
    QTRY_VERIFY(unlocked);
    QCOMPARE(relay.gate(), AdminRelay::GateUnlocked);

    statusDone = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o)
    {
        QCOMPARE(o.result.toObject()["job"].toObject()["id"].toString(), QStringLiteral("j1"));
        statusDone = true;
    });
    QTRY_VERIFY(statusDone);
    QCOMPARE(ws.adminFrames.last()["data"].toObject()["admin_session"].toString(), Session);

    //the password only ever went out in admin_unlock
    for (const QJsonObject &f: std::as_const(ws.adminFrames))
    {
        if (f["msg"] != "admin_unlock")
            QVERIFY(!QJsonDocument(f).toJson().contains(AdminPass.toUtf8()));
    }

    QSignalSpy progressSpy(&relay, &AdminRelay::upgradeProgress);
    ws.pushEvent(QJsonObject{{ "type", "upgrade_progress" },
                             { "data", QJsonObject{{ "status", "upgrading" }, { "total_percent", 10 }} }});
    QTRY_COMPARE(progressSpy.count(), 1);

    con.logout();
    QVERIFY(!relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
}

void TstAdminRelay::realConnectionServerDropKillsSession()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    FakeAdminWs ws(&tls);
    QVERIFY(ws.start());
    CalaosConnection con;
    TlsPinStore pins(dir.filePath(QStringLiteral("pins_drop.ini")));
    pins.setPin(TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws.port()),
                TlsPinning::spkiSha256(tls.localCertificate()));
    con.setPinStore(&pins);
    AdminRelay relay(&con);
    relay.bindConnection(&con);
    QSignalSpy up(&relay, &AdminRelay::transportUp);
    con.login(TestUser, TestPass, ws.url());
    QTRY_VERIFY(up.count() >= 1);

    bool probed = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &) { probed = true; });
    QTRY_VERIFY(probed);
    bool unlocked = false;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { unlocked = o.ok; });
    QTRY_VERIFY(unlocked);

    ws.stop();
    QTRY_VERIFY(!relay.isUnlocked());
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
    con.logout();
}

//B1: a box reached over plain ws (http://...:5454) never gets the password,
//and nothing seen on the previous box's socket lets it through.
void TstAdminRelay::realConnectionNoPasswordOverPlainWs()
{
    FakeAdminWs ws; //ws://
    QVERIFY(ws.start());
    CalaosConnection con;
    AdminRelay relay(&con);
    relay.bindConnection(&con);

    QSignalSpy up(&relay, &AdminRelay::transportUp);
    con.login(TestUser, TestPass, ws.url());
    QTRY_VERIFY(up.count() >= 1);
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
    QVERIFY(!con.isSecure());

    //even if a household answer arrived on this socket, the password stays home
    ws.mode = FakeAdminWs::Normal;
    bool probed = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &) { probed = true; });
    QTRY_VERIFY(probed);
    const int frames = ws.adminFrames.size();
    QString code;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, QStringLiteral("https_required"));
    QCOMPARE(relay.gate(), AdminRelay::GateHttpsRequired);
    QTest::qWait(100);
    QCOMPARE(ws.adminFrames.size(), frames);
    for (const QJsonObject &f: std::as_const(ws.adminFrames))
        QVERIFY(f["msg"] != "admin_unlock");
    con.logout();
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
}

void TstAdminRelay::realConnectionOldServer()
{
    FakeAdminWs ws;
    ws.mode = FakeAdminWs::Silent;
    QVERIFY(ws.start());
    CalaosConnection con;
    AdminRelay relay(&con);
    relay.bindConnection(&con);
    relay.setTimeouts(200, 400);
    QSignalSpy up(&relay, &AdminRelay::transportUp);
    con.login(TestUser, TestPass, ws.url());
    QTRY_VERIFY(up.count() >= 1);

    QString code;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, AdminRelay::ErrTimeout);
    QCOMPARE(relay.gate(), AdminRelay::GateOldServer);
    con.logout();
}

void TstAdminRelay::realConnectionHttpsRequired()
{
    FakeAdminWs ws;
    ws.mode = FakeAdminWs::HttpsRequired;
    QVERIFY(ws.start());
    CalaosConnection con;
    AdminRelay relay(&con);
    relay.bindConnection(&con);
    QSignalSpy up(&relay, &AdminRelay::transportUp);
    con.login(TestUser, TestPass, ws.url());
    QTRY_VERIFY(up.count() >= 1);

    QString code;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, QStringLiteral("https_required"));
    QCOMPARE(relay.gate(), AdminRelay::GateHttpsRequired);
    con.logout();
}

//---------------------------------------------------------- backend + screen

void TstAdminRelay::backendMapsReplies()
{
    AdminRelay::Outcome o;
    o.ok = false;
    o.error.code = "upgrade_in_progress";
    o.error.jobId = "j9";
    CalaosOsReply r = AdminRelayBackend::toReply(o);
    QVERIFY(!r.ok);
    QVERIFY(r.upgradeInProgress);
    QCOMPARE(r.httpStatus, 200);
    QCOMPARE(r.data.toObject()["job_id"].toString(), QStringLiteral("j9"));

    o.error = AdminRelay::Error();
    o.error.code = AdminRelay::ErrTimeout;
    r = AdminRelayBackend::toReply(o);
    QCOMPARE(r.httpStatus, 0);
    QCOMPARE(r.code, AdminRelay::ErrTimeout);

    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    bool done = false;
    backend.getUpdateStatus([&](bool okStatus, const CalaosOsProtocol::UpdateStatus &st)
    {
        QVERIFY(okStatus);
        QVERIFY(st.hasJobField);
        QVERIFY(st.job.valid);
        QCOMPARE(st.job.id, QStringLiteral("j2"));
        QVERIFY(st.job.rebootRequired);
        QVERIFY(st.job.finishedAt.isValid());
        QCOMPARE(st.progress["total_percent"].toInt(), 100);
        done = true;
    });
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "progress", progress("done", 100, 1, 1) }, { "job", job("j2", "done", true) }}));
    QVERIFY(done);
}

void TstAdminRelay::unlockReadsFullView()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);

    screen.activate();
    QCOMPARE(t.count("update_status"), 1);
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "progress", progress("idle", 0, 0, 0) }, { "job", QJsonValue::Null }}));
    QCOMPARE(screen.get_gate(), AdminRelay::GateLocked);
    QCOMPARE(screen.get_phase(), QStringLiteral("unknown"));

    screen.unlock(AdminPass);
    QCOMPARE(screen.get_busy(), QStringLiteral("unlock"));
    relay.handleReply("admin_unlock", t.frames.last().msgId, unlockOk());
    QCOMPARE(screen.get_busy(), QString());
    QVERIFY(screen.get_isUnlocked());
    QCOMPARE(screen.get_gate(), AdminRelay::GateUnlocked);

    //the full view: status with the session, the pending list, the box name
    const int st = t.last("update_status");
    QCOMPARE(t.frames[st].data["admin_session"].toString(), Session);
    const QStringList names{ "calaos-server", "calaos-home" };
    relay.handleReply("admin", t.frames[st].msgId,
                      ok(QJsonObject{{ "progress", progress("upgrading", 40, 2, 2, names) },
                                     { "job", job("j3", "running") }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("upgrading"));
    QCOMPARE(screen.get_currentPackage(), QStringLiteral("calaos-home"));
    QCOMPARE(screen.get_totalPercent(), 40);
    QCOMPARE(screen.get_pkgIndex(), 2);
    QCOMPARE(screen.progressRows()->rowCount(), 2);
    QVERIFY(screen.get_externalRun()); //not started from this screen

    const int info = t.last("system_info");
    QVERIFY(info >= 0);
    relay.handleReply("admin", t.frames[info].msgId, ok(QJsonObject{{ "hostname", "calaos-box" }}));
    QCOMPARE(screen.get_boxName(), QStringLiteral("calaos-box"));
    QVERIFY(t.last("update_available") >= 0);

    //a household event keeps the name read from the full view
    relay.handleEvent(QJsonObject{{ "type", "upgrade_progress" },
                                  { "data", progress("upgrading", 60, 2, 2) }});
    QCOMPARE(screen.get_totalPercent(), 60);
    QCOMPARE(screen.get_currentPackage(), QStringLiteral("calaos-home"));
}

void TstAdminRelay::checkMapsPackagesAndGroups()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    unlockRelay(t, relay);

    screen.checkUpdates();
    QCOMPARE(screen.get_busy(), QStringLiteral("update_check"));
    const int check = t.last("update_check");
    QJsonObject pkgs{
        { "calaos-server", QJsonObject{{ "name", "calaos-server" }, { "version", "4.6.0" }, { "current_version", "4.5.0" }} },
        { "linux-image-arm64", QJsonObject{{ "name", "linux-image-arm64" }, { "version", "6.12" }, { "current_version", "6.1" }} },
        { "libc6", QJsonObject{{ "name", "libc6" }, { "version", "2.41" }, { "current_version", "2.36" }} },
        { "openssl", QJsonObject{{ "name", "openssl" }, { "version", "3.5" }, { "current_version", "3.0" }} },
    };
    relay.handleReply("admin", t.frames[check].msgId, ok(pkgs));
    QCOMPARE(screen.get_busy(), QString());
    QCOMPARE(screen.get_phase(), QStringLiteral("available"));
    QCOMPARE(screen.get_updatesCount(), 4);

    QCOMPARE(screen.packages()->rowCount(), 1);
    UpdatePackageItem *server = screen.packages()->itemForPackage("calaos-server");
    QVERIFY(server);
    QCOMPARE(server->get_pkgInstalledVersion(), QStringLiteral("4.5.0"));
    QCOMPARE(server->get_pkgNewVersion(), QStringLiteral("4.6.0"));
    QVERIFY(server->get_pkgHasUpdate());

    UpdateGroupItem *kernel = screen.groups()->itemForGroup("kernel");
    UpdateGroupItem *system = screen.groups()->itemForGroup("system");
    QVERIFY(kernel && system);
    QCOMPARE(kernel->get_groupPkgCount(), 1);
    QCOMPARE(system->get_groupPkgCount(), 2);

    //nothing left: up to date, and the rows go
    screen.checkUpdates();
    relay.handleReply("admin", t.frames.last().msgId, ok(QJsonObject()));
    QCOMPARE(screen.get_phase(), QStringLiteral("upToDate"));
    QCOMPARE(screen.packages()->rowCount(), 0);
}

void TstAdminRelay::upgradeAllFollowsJobToReboot()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setPollInterval(60000);
    unlockRelay(t, relay);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", progress("idle", 0, 0, 0) }, { "job", QJsonValue::Null }}));

    screen.checkUpdates();
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "calaos-server", QJsonObject{{ "version", "2" }, { "current_version", "1" }} }}));
    screen.upgradeAll();
    const int up = t.last("update_upgrade_all");
    QVERIFY(up >= 0);
    QCOMPARE(t.frames[up].data["admin_session"].toString(), Session);
    relay.handleReply("admin", t.frames[up].msgId, ok(job("j4", "running")));
    QCOMPARE(screen.get_phase(), QStringLiteral("upgrading"));
    QVERIFY(!screen.get_externalRun());
    QCOMPARE(screen.get_totalPercent(), 0);

    const int statusBefore = t.count("update_status");
    relay.handleEvent(QJsonObject{{ "type", "upgrade_progress" }, { "data", progress("upgrading", 50, 1, 1) }});
    QCOMPARE(screen.get_totalPercent(), 50);
    //events carry no names: a new package index is read with the session
    QCOMPARE(t.count("update_status"), statusBefore + 1);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", progress("upgrading", 50, 1, 1, { "calaos-server" }) },
                                     { "job", job("j4", "running") }}));
    QCOMPARE(screen.get_currentPackage(), QStringLiteral("calaos-server"));
    //never rewound while followed
    relay.handleEvent(QJsonObject{{ "type", "upgrade_progress" }, { "data", progress("upgrading", 20, 1, 1) }});
    QCOMPARE(screen.get_totalPercent(), 50);

    //the end: update_status says how it ended
    const int before = t.count("update_status");
    relay.handleEvent(QJsonObject{{ "type", "upgrade_progress" }, { "data", progress("done", 100, 1, 1) }});
    QCOMPARE(t.count("update_status"), before + 1);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", QJsonObject{{ "status", "" }} }, { "job", job("j4", "done", true) }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("doneReboot"));

    screen.reboot();
    QCOMPARE(t.frames.last().data["action"].toString(), QStringLiteral("system_reboot"));
    relay.handleReply("admin", t.frames.last().msgId, ok(QJsonValue::Null));
    QCOMPARE(screen.get_info(), QStringLiteral("reboot_sent"));
}

void TstAdminRelay::householdViewHidesNames()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setPollInterval(60000);

    //even if a status carried names, none is shown without a session
    screen.activate();
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "progress", progress("upgrading", 30, 1, 2, { "calaos-server", "calaos-home" }) },
                                     { "job", job("j5", "running") }}));
    QCOMPARE(screen.get_gate(), AdminRelay::GateLocked);
    QCOMPARE(screen.get_phase(), QStringLiteral("upgrading"));
    QCOMPARE(screen.get_totalPercent(), 30);
    QCOMPARE(screen.get_currentPackage(), QString());
    QCOMPARE(screen.progressRows()->rowCount(), 0);
}

void TstAdminRelay::sessionLossClearsPackageList()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    unlockRelay(t, relay);
    screen.checkUpdates();
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "calaos-server", QJsonObject{{ "version", "2" }, { "current_version", "1" }} }}));
    QCOMPARE(screen.packages()->rowCount(), 1);

    relay.lock();
    QVERIFY(!screen.get_isUnlocked());
    QCOMPARE(screen.packages()->rowCount(), 0);
    QCOMPARE(screen.get_updatesCount(), 0);
    QCOMPARE(screen.get_phase(), QStringLiteral("unknown"));
}

void TstAdminRelay::rateLimitedCountdown()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    probeRelay(t, relay);

    screen.unlock("wrong");
    relay.handleReply("admin_unlock", t.frames.last().msgId,
                      err("rate_limited", QJsonObject{{ "retry_after", "2" }}));
    QCOMPARE(screen.get_retryAfter(), 2);
    QCOMPARE(screen.get_errorCode(), QStringLiteral("rate_limited"));
    QVERIFY(screen.get_errorText().contains("2"));

    const int sent = t.frames.size();
    screen.unlock(AdminPass);
    QCOMPARE(t.frames.size(), sent); //not before the delay

    QTRY_COMPARE_WITH_TIMEOUT(screen.get_retryAfter(), 0, 4000);
    QCOMPARE(screen.get_errorText(), QString());
    screen.unlock(AdminPass);
    QCOMPARE(t.frames.size(), sent + 1);
}

void TstAdminRelay::rebootDuringUpgradeRefused()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    unlockRelay(t, relay);
    screen.reboot();
    relay.handleReply("admin", t.frames.last().msgId, err("upgrade_in_progress", QJsonObject{{ "job_id", "j6" }}));
    QCOMPARE(screen.get_errorCode(), QStringLiteral("upgrade_in_progress"));
    QCOMPARE(screen.get_errorText(), AdminUpdates::errorTextFor("upgrade_in_progress", QString(), 0));
    QCOMPARE(screen.get_info(), QString());
}

void TstAdminRelay::blockedGateRetries()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setRetryDelays(50, 100);

    screen.activate();
    relay.handleReply("admin", t.frames.last().msgId, err("admin_unavailable"));
    QCOMPARE(screen.get_gate(), AdminRelay::GateUnavailable);
    QTRY_COMPARE(t.count("update_status"), 2);
    relay.handleReply("admin", t.frames.last().msgId, err("daemon_too_old"));
    QCOMPARE(screen.get_gate(), AdminRelay::GateTooOld);
    QTRY_COMPARE(t.count("update_status"), 3);
    relay.handleReply("admin", t.frames.last().msgId, ok(QJsonObject{{ "progress", QJsonObject() }}));
    QCOMPARE(screen.get_gate(), AdminRelay::GateLocked);

    //no retry once the screen is gone
    relay.request("update_status", QJsonObject(), {});
    screen.deactivate();
    relay.handleReply("admin", t.frames.last().msgId, err("admin_unavailable"));
    const int n = t.count("update_status");
    QTest::qWait(200);
    QCOMPARE(t.count("update_status"), n);
}

void TstAdminRelay::failedJobReason()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.activate();
    QJsonObject failed = job("j7", "error");
    failed["error"] = "interrupted";
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "progress", QJsonObject{{ "status", "idle" }} }, { "job", failed }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("failed"));
    QVERIFY(screen.get_failureText().contains("interrupted"));
}

void TstAdminRelay::gateIdleAfterTransportLost()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
    relay.handleTransportLost();
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);

    relay.request("update_status", QJsonObject(), {});
    relay.handleReply("admin", t.frames.last().msgId, err("https_required"));
    QCOMPARE(relay.gate(), AdminRelay::GateHttpsRequired);
    relay.handleTransportLost();
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
}

void TstAdminRelay::unlockOverPlainWsSendsNothing()
{
    FakeTransport t;
    t.secure = false;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    const int sent = t.frames.size();
    QString code;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, QStringLiteral("https_required"));
    QCOMPARE(t.frames.size(), sent);
    QCOMPARE(relay.gate(), AdminRelay::GateHttpsRequired);
}

void TstAdminRelay::unlockBeforeProbeOnNewSocketSendsNothing()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    //logout, then another login: the locked screen of the old socket is gone
    relay.handleTransportLost();
    relay.handleTransportUp();
    const int sent = t.frames.size();
    QString code;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    QTRY_COMPARE(code, AdminRelay::ErrNotReady);
    QCOMPARE(t.frames.size(), sent);
    QCOMPARE(relay.gate(), AdminRelay::GateIdle);
}

void TstAdminRelay::malformedSessionRefused()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    QString code;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &o) { code = o.error.code; });
    relay.handleReply("admin_unlock", t.frames.last().msgId,
                      QJsonObject{{ "success", "true" }, { "admin_session", "not-hex" }, { "expires_in", "900" }});
    QCOMPARE(code, AdminRelay::ErrUnknown);
    QVERIFY(!relay.isUnlocked());
}

void TstAdminRelay::timeoutWhileLockedIsUnreachable()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    probeRelay(t, relay);
    QCOMPARE(relay.gate(), AdminRelay::GateLocked);
    bool called = false;
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &) { called = true; });
    QTRY_VERIFY(called);
    QCOMPARE(relay.gate(), AdminRelay::GateUnreachable);
}

void TstAdminRelay::sessionLossFailsSessionRequests()
{
    FakeTransport t;
    AdminRelay relay(&t);
    unlockRelay(t, relay);
    QString withSession, household;
    relay.request("update_check", QJsonObject(), [&](const AdminRelay::Outcome &o) { withSession = o.error.code; });
    const QString checkId = t.frames.last().msgId;
    relay.lock();
    QCOMPARE(withSession, AdminRelay::ErrSessionLost);
    //a request made without the session is left alone
    relay.request("update_status", QJsonObject(), [&](const AdminRelay::Outcome &o) { household = o.ok ? "ok" : o.error.code; });
    relay.handleReply("admin", checkId, ok(QJsonObject{{ "calaos-server", QJsonObject() }}));
    QCOMPARE(withSession, AdminRelay::ErrSessionLost);
    relay.handleReply("admin", t.frames.last().msgId, ok(QJsonObject()));
    QCOMPARE(household, QStringLiteral("ok"));
}

void TstAdminRelay::checkAnswerAfterSessionLossDropped()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    unlockRelay(t, relay);
    screen.checkUpdates();
    const QString checkId = t.frames.last().msgId;
    relay.handleEvent(QJsonObject{{ "type", "admin_set" }});
    QCOMPARE(screen.get_busy(), QString());
    relay.handleReply("admin", checkId,
                      ok(QJsonObject{{ "calaos-server", QJsonObject{{ "version", "2" }, { "current_version", "1" }} }}));
    QCOMPARE(screen.packages()->rowCount(), 0);
    QCOMPARE(screen.get_updatesCount(), 0);
    QCOMPARE(screen.get_errorText(), QString());
    QCOMPARE(screen.get_info(), QStringLiteral("session_invalidated"));
}

void TstAdminRelay::sessionLossReducesToHouseholdView()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setPollInterval(60000);
    unlockRelay(t, relay);
    QJsonObject failed = job("j8", "error");
    failed["error"] = "dpkg returned an error code";
    failed["package"] = "calaos-server";
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", QJsonObject{{ "status", "error" }, { "error", "E: dpkg" },
                                                               { "current_pkg", "calaos-server" }} },
                                     { "job", failed }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("failed"));
    QVERIFY(screen.get_failureText().contains("dpkg returned"));

    relay.lock();
    QCOMPARE(screen.get_phase(), QStringLiteral("failed"));
    QVERIFY(!screen.get_failureText().contains("dpkg"));
    QCOMPARE(screen.get_currentPackage(), QString());
}

void TstAdminRelay::httpFallbackGate()
{
    FakeTransport t;
    t.up = false; //CalaosConnection sends nothing admin over HTTP
    t.httpFallback = true;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setRetryDelays(50, 100);
    screen.activate();
    QTRY_COMPARE(screen.get_gate(), AdminRelay::GateWebsocketRequired);
    QCOMPARE(screen.get_errorText(), QString());
    QTest::qWait(200);
    QVERIFY(t.frames.isEmpty()); //no retry loop either
}

void TstAdminRelay::rateLimitedOnActionCountsDown()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    unlockRelay(t, relay);
    screen.checkUpdates();
    relay.handleReply("admin", t.frames.last().msgId, err("rate_limited", QJsonObject{{ "retry_after", "7" }}));
    QCOMPARE(screen.get_retryAfter(), 7);
    QVERIFY(screen.get_errorText().contains("7"));
}

void TstAdminRelay::unansweredUpgradeReadsStatus()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setPollInterval(60000);
    unlockRelay(t, relay);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", progress("idle", 0, 0, 0) }, { "job", QJsonValue::Null }}));
    screen.checkUpdates();
    relay.handleReply("admin", t.frames.last().msgId,
                      ok(QJsonObject{{ "calaos-server", QJsonObject{{ "version", "2" }, { "current_version", "1" }} }}));

    //the reply never comes: the run may have started, update_status tells
    const int before = t.count("update_status");
    screen.upgradeAll();
    QTRY_COMPARE(t.count("update_status"), before + 1);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", progress("upgrading", 10, 1, 1, { "calaos-server" }) },
                                     { "job", job("j9", "running") }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("upgrading"));
}

void TstAdminRelay::rebootGuardedDuringUpgrade()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    screen.setPollInterval(60000);
    unlockRelay(t, relay);

    //refused by the box: the run started elsewhere is followed
    const int before = t.count("update_status");
    screen.reboot();
    relay.handleReply("admin", t.frames[t.last("system_reboot")].msgId,
                      err("upgrade_in_progress", QJsonObject{{ "job_id", "j10" }}));
    QCOMPARE(t.count("update_status"), before + 1);
    relay.handleReply("admin", t.frames[t.last("update_status")].msgId,
                      ok(QJsonObject{{ "progress", progress("upgrading", 5, 1, 1) }, { "job", job("j10", "running") }}));
    QCOMPARE(screen.get_phase(), QStringLiteral("upgrading"));

    //and no reboot is even sent while it runs
    const int reboots = t.count("system_reboot");
    screen.reboot();
    QCOMPARE(t.count("system_reboot"), reboots);
}

void TstAdminRelay::oldSocketErrorClearedOnReconnect()
{
    FakeTransport t;
    AdminRelay relay(&t);
    relay.setTimeouts(100, 300);
    AdminRelayBackend backend(&relay);
    AdminUpdates screen(&backend, &relay);
    probeRelay(t, relay);
    screen.unlock("typed");
    relay.handleTransportLost();
    QTRY_COMPARE(screen.get_errorCode(), QString()); //disconnected, then idle
    QCOMPARE(screen.get_gate(), AdminRelay::GateIdle);
    QCOMPARE(screen.get_errorText(), QString());

    //a rate limit survives the reconnect
    probeRelay(t, relay);
    screen.unlock("typed");
    relay.handleReply("admin_unlock", t.frames.last().msgId,
                      err("rate_limited", QJsonObject{{ "retry_after", "30" }}));
    relay.handleTransportLost();
    QCOMPARE(screen.get_errorCode(), QStringLiteral("rate_limited"));
    QVERIFY(screen.get_retryAfter() > 0);
}

void TstAdminRelay::replyForAnotherMessageIgnored()
{
    FakeTransport t;
    AdminRelay relay(&t);
    probeRelay(t, relay);
    bool called = false;
    relay.unlock(AdminPass, [&](const AdminRelay::Outcome &) { called = true; });
    const QString id = t.frames.last().msgId;
    //same msg_id, another message: not the answer to admin_unlock
    relay.handleReply("admin", id, ok(QJsonObject()));
    QVERIFY(!called);
    relay.handleReply("admin_unlock", id, unlockOk());
    QVERIFY(called);
    QVERIFY(relay.isUnlocked());
}

void TstAdminRelay::resumePolicyKeepsThePage()
{
    ResumePolicy p;
    //the password manager: page kept, reconnected in place
    p.resigned(true, false);
    QVERIFY(p.keepPage());
    QVERIFY(p.becameActive());
    QCOMPARE(p.phase(), ResumePolicy::Resuming);
    p.sessionFailed();
    QVERIFY(!p.keepPage()); //failed in place: the login screen as usual

    p.resigned(true, false);
    p.becameActive();
    p.loggedIn();
    QVERIFY(!p.keepPage());

    //the drop itself while in the background does not give the page up
    p.resigned(true, false);
    p.sessionFailed();
    QVERIFY(p.keepPage());
    p.reset();
    QVERIFY(!p.keepPage());

    //nothing to keep: the usual path
    p.resigned(false, false);
    QVERIFY(!p.keepPage());
    QVERIFY(p.becameActive());
    QVERIFY(!p.keepPage());

    //login fields edited when leaving (to copy the password): no automatic
    //login on return, which would replace the typed text
    p.resigned(false, true);
    QVERIFY(!p.becameActive());
    QVERIFY(!p.keepPage());
    QVERIFY(p.becameActive()); //the skip is for one return only

    //the skip wins over a kept page
    p.resigned(true, true);
    QVERIFY(!p.becameActive());
    QVERIFY(!p.keepPage());

    //a logout in between forgets it
    p.resigned(false, true);
    p.reset();
    QVERIFY(p.becameActive());
}

void TstAdminRelay::biometricsStoredOnlyWhenAsked()
{
    BiometricFixture f;
    f.openLocked();
    f.screen.unlock(AdminPass);
    f.answerUnlock();
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
    QVERIFY(f.bio.get_available());
}

void TstAdminRelay::biometricsStoredOnceTheBoxAccepted()
{
    BiometricFixture f;
    f.openLocked();
    f.bio.unlockAndKeep(QStringLiteral("typo"));
    QVERIFY(f.hw.vault.isEmpty());
    f.answerUnlock(err("bad_password"));
    QVERIFY(f.hw.vault.isEmpty());

    f.bio.unlockAndKeep(AdminPass);
    QCOMPARE(f.t.frames.last().data["admin_pass"].toString(), AdminPass);
    f.answerUnlock();
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
    QVERIFY(f.bio.get_stored());
}

void TstAdminRelay::biometricsNotStoredWithoutEnrolment()
{
    BiometricFixture f;
    f.hw.enrolled = false;
    f.openLocked();
    QVERIFY(!f.bio.get_available());
    f.bio.unlockAndKeep(AdminPass);
    f.answerUnlock();
    QVERIFY(f.relay.isUnlocked());
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(f.hw.deferredKey.isEmpty());
    QVERIFY(f.hw.reads.isEmpty());
}

void TstAdminRelay::biometricsStoreCanceledOrFailedLeavesNothing()
{
    for (int result: { int(HardwareUtils::BiometricCanceled), int(HardwareUtils::BiometricFailed) })
    {
        BiometricFixture f;
        f.hw.deferStore = true;
        f.openLocked();
        f.bio.unlockAndKeep(AdminPass);
        f.answerUnlock();
        f.hw.answerStore(result);
        QVERIFY(f.hw.vault.isEmpty());
        QVERIFY(!f.bio.get_stored());
        //the box comes back: nothing condemns the next try
        QVERIFY(f.bio.get_available());
    }
}

void TstAdminRelay::biometricsSecretNeverInSettings()
{
    BiometricFixture f;
    f.store();
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    const QStringList keys = settings.allKeys();
    bool sawBox = false;
    for (const QString &key: keys)
    {
        QVERIFY(!settings.value(key).toString().contains(AdminPass));
        sawBox |= key.startsWith(QStringLiteral("admin_biometrics/"));
    }
    QVERIFY(sawBox);
}

void TstAdminRelay::biometricsAskedWhenTheCardOpens()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    QCOMPARE(f.hw.reads, QStringList{ f.t.host });
    QVERIFY(f.bio.get_stored());
    QCOMPARE(f.unlocks(), 0);

    f.hw.answerRead();
    QCOMPARE(f.unlocks(), 1);
    QCOMPARE(f.t.frames.last().data["admin_pass"].toString(), AdminPass);
    f.answerUnlock();
    QVERIFY(f.screen.get_isUnlocked());
    QCOMPARE(f.hw.reads.size(), 1);
}

void TstAdminRelay::biometricsCancelLeavesTheField()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead(HardwareUtils::BiometricCanceled);
    QCOMPARE(f.unlocks(), 0);
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);

    //not asked again on this opening, the field is what is left
    f.relay.handleTransportLost();
    f.relay.handleTransportUp();
    f.screen.retry();
    f.answerProbe();
    QCOMPARE(f.relay.gate(), AdminRelay::GateLocked);
    QCOMPARE(f.hw.reads.size(), 1);

    f.screen.unlock(QStringLiteral("typed"));
    QCOMPARE(f.t.frames.last().data["admin_pass"].toString(), QStringLiteral("typed"));
}

void TstAdminRelay::biometricsFailedReadLeavesTheField()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead(HardwareUtils::BiometricFailed);
    QCOMPARE(f.unlocks(), 0);
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
    QVERIFY(f.bio.get_stored());
}

void TstAdminRelay::biometricsChangedEnrolmentForgets()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead(HardwareUtils::BiometricInvalidated);
    QCOMPARE(f.unlocks(), 0);
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
}

void TstAdminRelay::biometricsRefusedPasswordForgets()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead();
    f.answerUnlock(err("bad_password"));
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
    QCOMPARE(f.relay.gate(), AdminRelay::GateLocked);
    QCOMPARE(f.hw.reads.size(), 1);
}

void TstAdminRelay::biometricsTypedRefusalKeepsTheSecret()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead(HardwareUtils::BiometricCanceled);
    f.screen.unlock(QStringLiteral("typo"));
    f.answerUnlock(err("bad_password"));
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
}

void TstAdminRelay::biometricsOtherRefusalKeepsTheSecret()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead();
    f.answerUnlock(err("rate_limited"));
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
}

//iOS: the prompt resigns the app, which drops the socket.
void TstAdminRelay::biometricsReadWaitsForTheNewSocket()
{
    BiometricFixture f;
    f.holdWithoutSocket();
    QCOMPARE(f.unlocks(), 0);

    f.t.up = true;
    f.relay.handleTransportUp();
    f.screen.retry();
    QCOMPARE(f.unlocks(), 0);
    f.answerProbe();
    QCOMPARE(f.unlocks(), 1);
    QCOMPARE(f.hw.reads.size(), 1);
}

void TstAdminRelay::biometricsReadDroppedByTheTimer()
{
    BiometricFixture f;
    f.bio.setHoldDelay(50);
    f.holdWithoutSocket();
    QTest::qWait(120);
    f.socketBack();
    QCOMPARE(f.unlocks(), 0);
}

//qSleep: no event loop, as while suspended, where the timer does not run.
void TstAdminRelay::biometricsReadDroppedByTheClock()
{
    BiometricFixture f;
    f.bio.setHoldDelay(50);
    f.holdWithoutSocket();
    QTest::qSleep(120);
    f.t.up = true;
    f.relay.handleTransportUp();
    f.screen.retry();
    f.relay.handleReply("admin", f.t.frames.last().msgId,
                        ok(QJsonObject{{ "progress", QJsonObject() }, { "job", QJsonValue::Null }}));
    QCOMPARE(f.unlocks(), 0);
}

void TstAdminRelay::biometricsSuspensionDropsWhatIsHeld()
{
    for (Qt::ApplicationState state: { Qt::ApplicationSuspended, Qt::ApplicationHidden })
    {
        BiometricFixture read;
        read.holdWithoutSocket();
        emit qGuiApp->applicationStateChanged(state);
        read.socketBack();
        QCOMPARE(read.unlocks(), 0);

        //waiting for the answer of the box
        BiometricFixture typed;
        typed.openLocked();
        typed.bio.unlockAndKeep(AdminPass);
        typed.bio.applicationStateChanged(state);
        typed.answerUnlock();
        QVERIFY(typed.hw.deferredKey.isEmpty());
        QVERIFY(typed.hw.vault.isEmpty());

        //behind the Android prompt
        BiometricFixture storing;
        storing.hw.deferStore = true;
        storing.openLocked();
        storing.bio.unlockAndKeep(AdminPass);
        storing.answerUnlock();
        storing.bio.applicationStateChanged(state);
        QCOMPARE(storing.hw.canceledStores, 1);
        storing.hw.answerStore(HardwareUtils::BiometricOk);
        QVERIFY(storing.hw.vault.isEmpty());
        QVERIFY(!storing.bio.get_stored());
    }
}

void TstAdminRelay::biometricsInactiveKeepsTheRead()
{
    BiometricFixture f;
    f.holdWithoutSocket();
    f.bio.applicationStateChanged(Qt::ApplicationInactive);
    f.socketBack();
    QCOMPARE(f.unlocks(), 1);
}

void TstAdminRelay::biometricsNeverSentToAnotherBox()
{
    BiometricFixture f;
    f.holdWithoutSocket();
    f.t.host = QStringLiteral("box-b.example:443");
    f.socketBack();
    QCOMPARE(f.unlocks(), 0);
    QVERIFY(!f.bio.get_stored());
    QCOMPARE(f.hw.reads.size(), 1);
}

void TstAdminRelay::biometricsCertificateQuestionDropsTheRead()
{
    BiometricFixture f;
    f.store();
    f.t.trustPending = true;
    f.reopen();
    QVERIFY(f.hw.reads.isEmpty());

    f.t.trustPending = false;
    f.relay.handleTransportLost();
    f.relay.handleTransportUp();
    f.screen.retry();
    f.answerProbe();
    QCOMPARE(f.hw.reads.size(), 1);

    f.t.trustPending = true;
    f.hw.answerRead();
    QCOMPARE(f.unlocks(), 0);
    f.t.trustPending = false;
    f.relay.handleTransportLost();
    f.socketBack();
    QCOMPARE(f.unlocks(), 0);
}

//Through the signals of a real connection: nothing else proves the wiring.
void TstAdminRelay::biometricsCertificateSignalsOfTheConnection()
{
    CalaosConnection con;
    {
        BiometricFixture f;
        f.bio.bindConnection(&con);
        f.holdWithoutSocket();
        emit con.tlsTrustRequested(QVariantMap());
        f.socketBack();
        QCOMPARE(f.unlocks(), 0);
        QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
    }
    {
        BiometricFixture f;
        f.bio.bindConnection(&con);
        f.holdWithoutSocket();
        emit con.tlsTrustAccepted(f.t.host);
        QVERIFY(f.hw.vault.isEmpty());
        QVERIFY(!f.bio.get_stored());
        f.socketBack();
        QCOMPARE(f.unlocks(), 0);
    }
}

void TstAdminRelay::biometricsForgetDuringARead()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    const QString secret = f.hw.vault.value(f.t.host);
    const int before = f.unlocks();
    f.bio.forgetBox(f.t.host);
    f.hw.emitBiometricReadDone(f.t.host, HardwareUtils::BiometricOk, secret);
    QCOMPARE(f.unlocks(), before);

    BiometricFixture g;
    g.store();
    g.reopen();
    g.bio.forgetAll();
    g.hw.emitBiometricReadDone(g.t.host, HardwareUtils::BiometricOk, secret);
    QCOMPARE(g.unlocks(), 0);
}

//The store answers after the erase: it must not bring the secret back.
void TstAdminRelay::biometricsForgetDuringAStore()
{
    BiometricFixture f;
    f.hw.deferStore = true;
    f.openLocked();
    f.bio.unlockAndKeep(AdminPass);
    f.answerUnlock();
    f.bio.forgetAll();
    f.hw.answerStore(HardwareUtils::BiometricOk);
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
}

void TstAdminRelay::biometricsRowStaysWithoutEnrolment()
{
    BiometricFixture f;
    f.store();
    f.hw.enrolled = false;
    f.relay.handleTransportUp();
    QVERIFY(f.bio.get_stored());
    QVERIFY(!f.bio.get_available());
    f.bio.disable();
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
}

void TstAdminRelay::biometricsDisable()
{
    BiometricFixture f;
    f.store();
    f.reopen();
    f.hw.answerRead(HardwareUtils::BiometricCanceled);
    QVERIFY(f.bio.get_stored());
    f.bio.disable();
    QVERIFY(f.hw.vault.isEmpty());
    QVERIFY(!f.bio.get_stored());
    QVERIFY(f.bio.get_available());
}

void TstAdminRelay::biometricsForgetAllAndForgetBox()
{
    BiometricFixture f;
    f.store();
    f.bio.forgetBox(f.t.host);
    QVERIFY(f.hw.vault.isEmpty());
    f.reopen();
    QVERIFY(f.hw.reads.isEmpty());
    QVERIFY(!f.bio.get_stored());

    BiometricFixture g;
    g.holdWithoutSocket();
    g.hw.vault[QStringLiteral("box-b.example:443")] = QStringLiteral("other");
    g.bio.forgetAll();
    QVERIFY(g.hw.vault.isEmpty());
    g.socketBack();
    QCOMPARE(g.unlocks(), 0);
    QVERIFY(!g.bio.get_stored());
}

//The same calls as Application; the automatic logout erases nothing.
void TstAdminRelay::biometricsWipeOnUserActions()
{
    TlsPinStore pins(dir.filePath(QStringLiteral("pins_wipe.ini")));

    BiometricFixture logout;
    logout.store();
    int logouts = 0;
    SecretWipe::userLogout(&logout.hw, &logout.bio, [&]() { logouts++; });
    QCOMPARE(logouts, 1);
    QVERIFY(logout.hw.vault.isEmpty());

    BiometricFixture reset;
    reset.store();
    SecretWipe::resetCredentials(&reset.hw, &reset.bio);
    QVERIFY(reset.hw.vault.isEmpty());

    BiometricFixture pin;
    pin.store();
    pins.setPin(pin.t.host, QStringLiteral("00"));
    SecretWipe::forgetTlsPin(&pins, &pin.bio, pin.t.host);
    QVERIFY(pin.hw.vault.isEmpty());
    QVERIFY(pins.pin(pin.t.host).isEmpty());

    CalaosConnection con;
    BiometricFixture dropped;
    dropped.bio.bindConnection(&con);
    dropped.store();
    con.logout();
    dropped.relay.handleTransportLost();
    QCOMPARE(dropped.hw.vault.value(dropped.t.host), AdminPass);
}

void TstAdminRelay::biometricsFirstLaunchEmptiesTheStore()
{
    BiometricFixture f;
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.remove(QStringLiteral("admin_biometrics_seen"));
    settings.sync();

    f.hw.vault[f.t.host] = QStringLiteral("left by a removed install");
    {
        AdminBiometrics first(&f.hw, &f.relay, &f.screen, &f.t);
        QVERIFY(f.hw.vault.isEmpty());
    }
    f.hw.vault[f.t.host] = AdminPass;
    AdminBiometrics second(&f.hw, &f.relay, &f.screen, &f.t);
    QCOMPARE(f.hw.vault.value(f.t.host), AdminPass);
}

//At exit the updates page goes first.
void TstAdminRelay::biometricsOutlivesTheUpdatesPage()
{
    FakeTransport t;
    AdminRelay relay(&t);
    AdminRelayBackend backend(&relay);
    FakeBiometricHw hw;
    auto *screen = new AdminUpdates(&backend, &relay);
    {
        AdminBiometrics bio(&hw, &relay, screen, &t);
        delete screen;
    }
    QVERIFY(true);
}

QTEST_MAIN(TstAdminRelay)
#include "tst_adminrelay.moc"
