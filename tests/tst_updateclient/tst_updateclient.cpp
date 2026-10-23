/*
 * The kiosk update client against an in-process fake calaos-container daemon.
 *
 * A real CalaosOsAPI and a real UpdateManager talk to FakeDaemon, which
 * serves the HTTP routes and the events WebSocket on one 127.0.0.1 port, like
 * the daemon. It follows docs/API.md of calaos-container: /api/v2 envelope and
 * error codes, 202 upgrade jobs with upgrade_job events, and, when told to be
 * an old daemon (1.8.0), plain-text 404 on every /api/v2 route and the
 * legacy synchronous routes.
 *
 * Covered:
 *  - bearer in the Authorization header everywhere, the events WebSocket
 *    included, never ?token= in a URL, never the token in the logs;
 *  - v2 upgrade: 202 is not the end, only the terminal job state is (event,
 *    or the update/status safety net when the event is lost); reboot_required
 *    from the job; a group queue of jobs; the daemon restarting under a job;
 *  - legacy fallback on a plain 404 only, cached; an enveloped 404 is not an
 *    old daemon; a v2 daemon without jobs upgrades on the legacy route;
 *  - 401: the token file is re-read and the call retried once;
 *  - 409 upgrade_in_progress on upgrade (adopts the running job), on check,
 *    and on reboot, on both surfaces.
 *
 * Every test function builds its own daemon, client and token: each can run
 * alone.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QWebSocket>
#include <QWebSocketServer>
#include <qfappdispatcher.h>

#include "CalaosOsAPI.h"
#include "UpdateManager.h"

static QStringList s_logLines;
static QtMessageHandler s_previousHandler = nullptr;

static void captureMessages(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    s_logLines << msg;
    if (s_previousHandler)
        s_previousHandler(type, ctx, msg);
}

struct HttpRequest
{
    QByteArray method;
    QString path;
    QString query;
    QByteArray authorization;
    QByteArray body;
};

class FakeDaemon: public QObject
{
    Q_OBJECT

public:
    FakeDaemon():
        wsServer(QStringLiteral("fake calaos-container"), QWebSocketServer::NonSecureMode)
    {
        connect(&server, &QTcpServer::newConnection, this, &FakeDaemon::onNewConnection);
        connect(&wsServer, &QWebSocketServer::newConnection, this, [this]()
        {
            while (QWebSocket *ws = wsServer.nextPendingConnection())
            {
                clients.append(ws);
                connect(ws, &QWebSocket::disconnected, this, [this, ws]()
                {
                    clients.removeAll(ws);
                    ws->deleteLater();
                });
                //on connect: one upgrade_progress snapshot first
                sendTo(ws, QStringLiteral("upgrade_progress"), progress);
            }
        });

        available = QJsonObject{
            { "calaos-server", QJsonObject{{ "name", "calaos-server" }, { "version", "4.5.0" }, { "current_version", "4.4.0" }} },
        };
    }

    ~FakeDaemon()
    {
        stop();
    }

    //behaviour
    QString token;
    bool hasV2 = true;
    bool hasJobs = true;

    //state
    QJsonObject available;
    QJsonObject progress{{ "status", "idle" }};
    QJsonValue job = QJsonValue(QJsonValue::Null);
    bool busy = false;
    QString busyJobId; //job_id of 409 answers, empty: none
    int reboots = 0;
    int legacyUpgrades = 0;

    //what the kiosk sent
    QList<HttpRequest> requests;
    QList<HttpRequest> wsHandshakes;
    QList<QWebSocket *> clients;

    bool start(quint16 port = 0)
    {
        if (!server.listen(QHostAddress::LocalHost, port))
            return false;
        m_port = server.serverPort();
        return true;
    }

    //the daemon goes away (restart after a self-upgrade): every connection drops
    void stop()
    {
        for (QWebSocket *ws: clients)
        {
            ws->disconnect(this);
            ws->abort();
            ws->deleteLater();
        }
        clients.clear();
        for (QTcpSocket *s: m_http.keys())
        {
            s->disconnect(this);
            s->abort();
            s->deleteLater();
        }
        m_http.clear();
        server.close();
    }

    QString baseAddr() const
    {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_port);
    }

    int count(const QByteArray &method, const QString &path) const
    {
        int n = 0;
        for (const HttpRequest &r: requests)
            if (r.method == method && r.path == path)
                n++;
        return n;
    }

    int countPrefix(const QString &prefix) const
    {
        int n = 0;
        for (const HttpRequest &r: requests)
            if (r.path.startsWith(prefix))
                n++;
        return n;
    }

    void sendEvent(const QString &type, const QJsonValue &data)
    {
        for (QWebSocket *ws: clients)
            sendTo(ws, type, data);
    }

    void setProgress(const QJsonObject &p, bool publish = true)
    {
        progress = p;
        if (publish)
            sendEvent(QStringLiteral("upgrade_progress"), progress);
    }

    QJsonObject startJob(const QString &kind, const QString &package)
    {
        QJsonObject j{{ "id", QStringLiteral("%1").arg(++m_jobs, 16, 16, QLatin1Char('0')) },
                      { "kind", kind },
                      { "status", "running" },
                      { "reboot_required", false },
                      { "daemon_restart", false },
                      { "started_at", "2026-10-23T09:00:00Z" }};
        if (!package.isEmpty())
            j["package"] = package;
        job = j;
        busy = true;
        busyJobId = j["id"].toString();
        progress = QJsonObject{{ "status", "upgrading" }, { "total_percent", 0 }};
        sendEvent(QStringLiteral("upgrade_job"), j);
        return j;
    }

    enum class Publish { All, None };

    void finishJob(const QString &status, bool rebootRequired, Publish publish = Publish::All)
    {
        QJsonObject j = job.toObject();
        j["status"] = status;
        j["reboot_required"] = rebootRequired;
        j["finished_at"] = "2026-10-23T09:05:00Z";
        if (status == QStringLiteral("error"))
            j["error"] = "upgrade failed: calaos-server";
        job = j;
        busy = false;
        busyJobId.clear();
        progress = QJsonObject{{ "status", status }, { "total_percent", 100 }};

        if (publish == Publish::All)
        {
            sendEvent(QStringLiteral("upgrade_progress"), progress);
            sendEvent(QStringLiteral("upgrade_job"), j);
        }
        //back to rest; the trailing idle frame is published too
        progress = QJsonObject{{ "status", "idle" }};
        if (publish == Publish::All)
            sendEvent(QStringLiteral("upgrade_progress"), progress);
    }

private slots:
    void onNewConnection()
    {
        while (QTcpSocket *s = server.nextPendingConnection())
        {
            m_http.insert(s, QByteArray());
            connect(s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead(s); });
            connect(s, &QTcpSocket::disconnected, this, [this, s]()
            {
                m_http.remove(s);
                s->deleteLater();
            });
        }
    }

private:
    QTcpServer server;
    QWebSocketServer wsServer;
    QHash<QTcpSocket *, QByteArray> m_http;
    quint16 m_port = 0;
    int m_jobs = 0x3f9c0000;

    static void sendTo(QWebSocket *ws, const QString &type, const QJsonValue &data)
    {
        const QJsonObject o{{ "type", type }, { "data", data }};
        ws->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }

    static bool parseHead(const QByteArray &raw, HttpRequest &req, int &contentLength, int &headLen)
    {
        headLen = raw.indexOf("\r\n\r\n");
        if (headLen < 0)
            return false;
        const QList<QByteArray> lines = raw.left(headLen).split('\n');
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        req.method = first.value(0);
        const QUrl url(QString::fromLatin1(first.value(1)));
        req.path = url.path();
        req.query = url.query();
        contentLength = 0;
        for (int i = 1; i < lines.size(); i++)
        {
            const QByteArray line = lines.at(i).trimmed();
            const int c = line.indexOf(':');
            if (c < 0)
                continue;
            const QByteArray key = line.left(c).trimmed().toLower();
            const QByteArray value = line.mid(c + 1).trimmed();
            if (key == "authorization")
                req.authorization = value;
            else if (key == "content-length")
                contentLength = value.toInt();
            else if (key == "upgrade" && value.toLower() == "websocket")
                req.method = "WS";
        }
        headLen += 4;
        return true;
    }

    void onReadyRead(QTcpSocket *s)
    {
        //peek first: a WebSocket handshake must stay in the socket for QWebSocketServer
        HttpRequest req;
        int contentLength = 0, headLen = 0;
        const QByteArray raw = s->peek(s->bytesAvailable());
        if (!parseHead(raw, req, contentLength, headLen))
            return;

        if (req.method == "WS")
        {
            s->disconnect(this);
            m_http.remove(s);
            wsHandshakes.append(req);

            if (!authorized(req))
            {
                reply(s, 401, "text/plain", "Unauthorized");
                return;
            }
            if (req.path != QStringLiteral("/api/events") &&
                !(hasV2 && req.path == QStringLiteral("/api/v2/events")))
            {
                reply(s, 404, "text/plain", "Cannot GET " + req.path.toUtf8());
                return;
            }
            wsServer.handleConnection(s);
            return;
        }

        if (raw.size() < headLen + contentLength)
            return;
        s->read(headLen + contentLength);
        req.body = raw.mid(headLen, contentLength);
        requests.append(req);
        route(s, req);
    }

    bool authorized(const HttpRequest &req) const
    {
        const QList<QByteArray> parts = req.authorization.split(' ');
        return !token.isEmpty() && parts.size() == 2 &&
               parts.at(0).toLower() == "bearer" && parts.at(1) == token.toUtf8();
    }

    static void reply(QTcpSocket *s, int status, const QByteArray &type, const QByteArray &body)
    {
        QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " X\r\n"
                         "Content-Type: " + type + "\r\n"
                         "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                         "Connection: close\r\n\r\n" + body;
        s->write(out);
        s->disconnectFromHost();
    }

    static void json(QTcpSocket *s, int status, const QJsonValue &v)
    {
        const QByteArray body = v.isObject() ?
                                    QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact) :
                                    QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact);
        reply(s, status, "application/json", body);
    }

    static void ok(QTcpSocket *s, int status, const QJsonValue &data, const QString &msg = QStringLiteral("ok"))
    {
        json(s, status, QJsonObject{{ "error", false }, { "msg", msg }, { "data", data }});
    }

    static void err(QTcpSocket *s, int status, const QString &code, const QString &msg, const QJsonValue &data = QJsonValue())
    {
        QJsonObject o{{ "error", true }, { "code", code }, { "msg", msg }};
        if (!data.isUndefined())
            o["data"] = data;
        json(s, status, o);
    }

    void busyError(QTcpSocket *s)
    {
        err(s, 409, "upgrade_in_progress", "an upgrade is already in progress",
            busyJobId.isEmpty() ? QJsonValue() : QJsonValue(QJsonObject{{ "job_id", busyJobId }}));
    }

    void route(QTcpSocket *s, const HttpRequest &req)
    {
        const bool v2 = req.path.startsWith(QStringLiteral("/api/v2/"));

        //the bearer middleware runs before routing, old daemons included
        if (!authorized(req))
        {
            if (v2 && hasV2)
                err(s, 401, "bad_bearer", "unauthorized");
            else
                json(s, 401, QJsonObject{{ "error", true }, { "msg", "wrong token" }});
            return;
        }

        if (v2 && !hasV2)
        {
            reply(s, 404, "text/plain; charset=utf-8", "Cannot " + req.method + " " + req.path.toUtf8());
            return;
        }

        if (v2)
            routeV2(s, req, req.path.mid(7));
        else
            routeLegacy(s, req);
    }

    void routeV2(QTcpSocket *s, const HttpRequest &req, const QString &p)
    {
        if (req.method == "GET" && p == QStringLiteral("/update/check"))
        {
            if (busy)
                busyError(s);
            else
                ok(s, 200, available);
        }
        else if (req.method == "GET" && p == QStringLiteral("/update/available"))
            ok(s, 200, available);
        else if (req.method == "GET" && p == QStringLiteral("/update/installed"))
            ok(s, 200, QJsonObject());
        else if (req.method == "GET" && p == QStringLiteral("/update/status"))
        {
            QJsonObject d{{ "progress", progress }};
            if (hasJobs)
                d["job"] = job;
            ok(s, 200, d);
        }
        else if (req.method == "POST" && (p == QStringLiteral("/update/upgrade-all") || p == QStringLiteral("/update/upgrade")))
        {
            if (!hasJobs)
            {
                err(s, 404, "not_found", "not found");
                return;
            }
            if (busy)
            {
                busyError(s);
                return;
            }
            QString pkg;
            if (p == QStringLiteral("/update/upgrade"))
            {
                pkg = QJsonDocument::fromJson(req.body).object()["package"].toString();
                if (!available.contains(pkg))
                {
                    err(s, 404, "package_not_found", "the package has no pending update");
                    return;
                }
            }
            const QJsonObject j = startJob(pkg.isEmpty() ? QStringLiteral("upgrade_all") : QStringLiteral("upgrade"), pkg);
            ok(s, 202, j, QStringLiteral("accepted"));
        }
        else if (req.method == "POST" && p == QStringLiteral("/system/reboot"))
        {
            if (busy)
                busyError(s);
            else
            {
                reboots++;
                ok(s, 200, QJsonValue::Null);
            }
        }
        else
            err(s, 404, "not_found", "not found");
    }

    void routeLegacy(QTcpSocket *s, const HttpRequest &req)
    {
        const QString &p = req.path;
        const QJsonObject inProgress{{ "error", true }, { "msg", "upgrade already in progress" }};

        if (req.method == "GET" && p == QStringLiteral("/api/update/check"))
            json(s, busy ? 500 : 200, busy ? inProgress : available);
        else if (req.method == "GET" && p == QStringLiteral("/api/update/available"))
            json(s, 200, available);
        else if (req.method == "GET" && p == QStringLiteral("/api/update/installed"))
            json(s, 200, QJsonObject());
        else if (req.method == "GET" && p == QStringLiteral("/api/update/status"))
            json(s, 200, progress);
        else if (req.method == "POST" && p == QStringLiteral("/api/update/upgrade-all"))
        {
            if (busy)
                json(s, 500, inProgress);
            else
            {
                legacyUpgrades++;
                json(s, 200, QJsonObject{{ "msg", "Upgrade done" }});
            }
        }
        else if (req.method == "POST" && p == QStringLiteral("/api/update/upgrade"))
        {
            const QString pkg = QUrlQuery(req.query).queryItemValue("package");
            if (busy)
                json(s, 500, inProgress);
            else if (!available.contains(pkg))
                json(s, 500, QJsonObject{{ "error", true }, { "msg", "package not found" }});
            else
            {
                legacyUpgrades++;
                json(s, 200, QJsonObject{{ "msg", "Upgrade done" }});
            }
        }
        else if (req.method == "POST" && p == QStringLiteral("/api/system/reboot"))
        {
            if (busy)
                json(s, 409, inProgress);
            else
            {
                reboots++;
                json(s, 200, QJsonObject{{ "error", false }, { "msg", "ok" }});
            }
        }
        else
            reply(s, 404, "text/plain; charset=utf-8", "Cannot " + req.method + " " + p.toUtf8());
    }
};

struct Dispatched
{
    QString type;
    QVariantMap message;
};

class TstUpdateClient: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    void bearerHeaderEverywhereAndNoTokenInUrls();
    void v2UpgradeAllEndsOnlyOnTerminalJob();
    void v2JobErrorIsAFailure();
    void v2LostTerminalEventIsCaughtByStatusPoll();
    void v2RebootRequiredComesFromTheJob();
    void v2GroupQueueRunsOneJobPerPackage();
    void v2DaemonRestartDuringJobReconnectsAndRereadsToken();
    void v2JobStartedElsewhereIsAdopted();

    void legacyDaemonFallsBackOnPlain404AndCachesIt();
    void legacyDaemonUpgradeStaysSynchronous();
    void enveloped404IsNotAnOldDaemon();
    void v2DaemonWithoutJobsUpgradesOnLegacyRoute();

    void unauthorizedRereadsTokenAndRetriesOnce();
    void unauthorizedTwiceFails();

    void upgradeInProgressAdoptsTheRunningJob();
    void upgradeInProgressWithoutJobIdFails();
    void checkDuringUpgradeKeepsDaemonAvailable();
    void rebootDuringUpgradeIsRefused_data();
    void rebootDuringUpgradeIsRefused();

private:
    void writeToken(const QString &value);
    void startClient(FakeDaemon &daemon);
    QList<Dispatched> dispatchedOf(const QString &type) const;

    QTemporaryDir m_dir;
    QString m_tokenPath;
    QStringList m_tokens; //every token of the test, checked against the logs
    QQmlApplicationEngine *m_engine = nullptr;
    CalaosOsAPI *m_api = nullptr;
    UpdateManager *m_mgr = nullptr;
    QList<Dispatched> m_dispatched;
};

void TstUpdateClient::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_tokenPath = m_dir.filePath(QStringLiteral("calaos-ct.token"));
    s_previousHandler = qInstallMessageHandler(captureMessages);
}

void TstUpdateClient::cleanup()
{
    //the manager first, then its client without spinning the event loop in
    //between: no reply can reach a deleted manager
    delete m_mgr;
    m_mgr = nullptr;
    delete m_api;
    m_api = nullptr;
    delete m_engine;
    m_engine = nullptr;
    m_dispatched.clear();

    for (const QString &t: std::as_const(m_tokens))
        for (const QString &line: std::as_const(s_logLines))
            QVERIFY2(!line.contains(t), qPrintable(QStringLiteral("token leaked in log: ") + line.left(120)));
    m_tokens.clear();
    s_logLines.clear();
}

void TstUpdateClient::writeToken(const QString &value)
{
    QFile f(m_tokenPath);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(value.toUtf8() + "\n");
    m_tokens << value;
}

static QString makeToken(char c)
{
    return QString(64, QLatin1Char(c));
}

void TstUpdateClient::startClient(FakeDaemon &daemon)
{
    m_engine = new QQmlApplicationEngine;
    connect(QFAppDispatcher::instance(m_engine), &QFDispatcher::dispatched, this,
            [this](QString type, QJSValue message)
    {
        m_dispatched.append({ type, message.toVariant().toMap() });
    });

    m_api = new CalaosOsAPI;
    m_api->setBaseAddr(daemon.baseAddr());
    m_api->setTokenFile(m_tokenPath);
    m_mgr = new UpdateManager(m_engine, nullptr, m_api);
}

QList<Dispatched> TstUpdateClient::dispatchedOf(const QString &type) const
{
    QList<Dispatched> out;
    for (const Dispatched &d: m_dispatched)
        if (d.type == type)
            out << d;
    return out;
}

void TstUpdateClient::bearerHeaderEverywhereAndNoTokenInUrls()
{
    FakeDaemon daemon;
    daemon.token = makeToken('a');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);

    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);
    QTRY_VERIFY(m_mgr->get_installedListAvailable());
    QCOMPARE(m_api->apiVersion(), CalaosOsProtocol::ApiVersion::V2);

    QVERIFY(!daemon.requests.isEmpty());
    for (const HttpRequest &r: std::as_const(daemon.requests))
    {
        QCOMPARE(r.authorization, QByteArray("Bearer ") + daemon.token.toUtf8());
        QVERIFY2(!r.query.contains(QStringLiteral("token")), qPrintable(r.path + "?" + r.query));
        QVERIFY(r.path.startsWith(QStringLiteral("/api/v2/")));
    }

    QCOMPARE(daemon.wsHandshakes.size(), 1);
    QCOMPARE(daemon.wsHandshakes.first().path, QStringLiteral("/api/v2/events"));
    QCOMPARE(daemon.wsHandshakes.first().query, QString());
    QCOMPARE(daemon.wsHandshakes.first().authorization, QByteArray("Bearer ") + daemon.token.toUtf8());
}

void TstUpdateClient::v2UpgradeAllEndsOnlyOnTerminalJob()
{
    FakeDaemon daemon;
    daemon.token = makeToken('b');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    m_mgr->upgradeAll();
    QVERIFY(m_mgr->get_isUpgrading());
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);

    //the 202 is only an acceptance
    QTest::qWait(300);
    QVERIFY(m_mgr->get_isUpgrading());
    QVERIFY(dispatchedOf("showNotificationMsg").isEmpty());

    daemon.setProgress(QJsonObject{{ "status", "upgrading" }, { "total_percent", 40 },
                                   { "current_pkg", "calaos-server" }, { "pkg_index", 1 }, { "pkg_count", 1 }});
    QTRY_COMPARE(m_mgr->get_totalPercent(), 40);
    QCOMPARE(m_mgr->get_currentPackage(), QStringLiteral("calaos-server"));

    //a terminal progress frame alone does not end a job
    daemon.sendEvent(QStringLiteral("upgrade_progress"), QJsonObject{{ "status", "done" }, { "total_percent", 100 }});
    QTest::qWait(300);
    QVERIFY(m_mgr->get_isUpgrading());

    daemon.finishJob(QStringLiteral("done"), false);
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QCOMPARE(dispatchedOf("showNotificationMsg").size(), 1);
    QVERIFY(dispatchedOf("showRebootDialog").isEmpty());
    QVERIFY(!m_mgr->get_rebootRequired());
}

void TstUpdateClient::v2JobErrorIsAFailure()
{
    FakeDaemon daemon;
    daemon.token = makeToken('c');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    m_mgr->upgradeAll();
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    daemon.finishJob(QStringLiteral("error"), false);

    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("error"));
    QCOMPARE(m_mgr->get_lastErrorString(), QStringLiteral("upgrade failed: calaos-server"));
}

void TstUpdateClient::v2LostTerminalEventIsCaughtByStatusPoll()
{
    FakeDaemon daemon;
    daemon.token = makeToken('d');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    m_mgr->setJobPollInterval(300);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    m_mgr->upgradeAll();
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    QTest::qWait(100);

    //every event of the end is lost: only update/status tells
    const int statusReads = daemon.count("GET", "/api/v2/update/status");
    daemon.finishJob(QStringLiteral("done"), false, FakeDaemon::Publish::None);

    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QVERIFY(daemon.count("GET", "/api/v2/update/status") > statusReads);
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
}

void TstUpdateClient::v2RebootRequiredComesFromTheJob()
{
    FakeDaemon daemon;
    daemon.token = makeToken('e');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //calaos-server only: the package names alone would never ask for a reboot
    m_mgr->upgradeAll();
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    daemon.finishJob(QStringLiteral("done"), true);

    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QVERIFY(m_mgr->get_rebootRequired());
    const QList<Dispatched> dialogs = dispatchedOf("showRebootDialog");
    QCOMPARE(dialogs.size(), 1);
    QCOMPARE(dialogs.first().message.value("showMachine").toBool(), true);
    QCOMPARE(dialogs.first().message.value("showApp").toBool(), false);
}

void TstUpdateClient::v2GroupQueueRunsOneJobPerPackage()
{
    FakeDaemon daemon;
    daemon.token = makeToken('f');
    writeToken(daemon.token);
    daemon.available = QJsonObject{
        { "linux-image-amd64", QJsonObject{{ "name", "linux-image-amd64" }, { "version", "6.1" }, { "current_version", "6.0" }} },
        { "firmware-linux", QJsonObject{{ "name", "firmware-linux" }, { "version", "2" }, { "current_version", "1" }} },
    };
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 2);

    m_mgr->upgradeGroup(QStringLiteral("kernel"));
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade"), 1);
    const QString first = QJsonDocument::fromJson(daemon.requests.last().body).object()["package"].toString();
    QVERIFY(daemon.available.contains(first));

    //the next package waits for the end of the first job
    QTest::qWait(300);
    QCOMPARE(daemon.count("POST", "/api/v2/update/upgrade"), 1);

    daemon.finishJob(QStringLiteral("done"), false);
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade"), 2);
    QVERIFY(m_mgr->get_isUpgrading());
    const QString second = QJsonDocument::fromJson(daemon.requests.last().body).object()["package"].toString();
    QVERIFY(second != first);

    daemon.finishJob(QStringLiteral("done"), true);
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    const QList<Dispatched> dialogs = dispatchedOf("showRebootDialog");
    QCOMPARE(dialogs.size(), 1);
    QCOMPARE(dialogs.first().message.value("showMachine").toBool(), true);
}

void TstUpdateClient::v2DaemonRestartDuringJobReconnectsAndRereadsToken()
{
    FakeDaemon daemon;
    daemon.token = makeToken('1');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    m_mgr->upgradeAll();
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    QTest::qWait(100);

    //the daemon upgraded itself: it ends the job, exits and comes back with
    //a new token file (a reboot-less upgrade keeps it, but the kiosk must not rely on it)
    const quint16 port = QUrl(daemon.baseAddr()).port();
    daemon.finishJob(QStringLiteral("done"), false, FakeDaemon::Publish::None);
    daemon.stop();
    QTest::qWait(300);
    QVERIFY(m_mgr->get_isUpgrading());

    daemon.token = makeToken('2');
    writeToken(daemon.token);
    daemon.wsHandshakes.clear();
    QVERIFY(daemon.start(port));

    QTRY_VERIFY_WITH_TIMEOUT(!m_mgr->get_isUpgrading(), 10000);
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QTRY_VERIFY(!daemon.wsHandshakes.isEmpty());
    QCOMPARE(daemon.wsHandshakes.last().authorization, QByteArray("Bearer ") + daemon.token.toUtf8());
    QCOMPARE(dispatchedOf("showNotificationMsg").size(), 1);
}

void TstUpdateClient::v2JobStartedElsewhereIsAdopted()
{
    FakeDaemon daemon;
    daemon.token = makeToken('3');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //the webapp starts an upgrade
    daemon.startJob(QStringLiteral("upgrade_all"), QString());
    QTRY_VERIFY(m_mgr->get_isUpgrading());
    daemon.setProgress(QJsonObject{{ "status", "upgrading" }, { "total_percent", 10 }});
    QTRY_COMPARE(m_mgr->get_totalPercent(), 10);

    daemon.finishJob(QStringLiteral("done"), false);
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QCOMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 0);
}

void TstUpdateClient::legacyDaemonFallsBackOnPlain404AndCachesIt()
{
    FakeDaemon daemon;
    daemon.token = makeToken('4');
    daemon.hasV2 = false;
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);

    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);
    QTRY_VERIFY(m_mgr->get_installedListAvailable());
    QCOMPARE(m_api->apiVersion(), CalaosOsProtocol::ApiVersion::Legacy);
    QVERIFY(daemon.count("GET", "/api/update/available") >= 1);

    //legacy events route, header auth, no ?token=
    QCOMPARE(daemon.wsHandshakes.last().path, QStringLiteral("/api/events"));
    QCOMPARE(daemon.wsHandshakes.last().query, QString());
    QCOMPARE(daemon.wsHandshakes.last().authorization, QByteArray("Bearer ") + daemon.token.toUtf8());

    //cached: a new call goes straight to the legacy route
    const int v2Calls = daemon.countPrefix(QStringLiteral("/api/v2/"));
    m_mgr->refresh();
    QTRY_COMPARE(daemon.count("GET", "/api/update/check"), 1);
    QTRY_VERIFY(!m_mgr->get_isChecking());
    QCOMPARE(daemon.countPrefix(QStringLiteral("/api/v2/")), v2Calls);
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
}

void TstUpdateClient::legacyDaemonUpgradeStaysSynchronous()
{
    FakeDaemon daemon;
    daemon.token = makeToken('5');
    daemon.hasV2 = false;
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);
    QTRY_COMPARE(m_api->apiVersion(), CalaosOsProtocol::ApiVersion::Legacy);

    //the legacy POST answers when apt ends: its 200 finishes the upgrade
    m_mgr->upgradeAll();
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(daemon.legacyUpgrades, 1);
    QCOMPARE(daemon.count("POST", "/api/update/upgrade-all"), 1);
    QCOMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 0);
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QCOMPARE(dispatchedOf("showNotificationMsg").size(), 1);
}

void TstUpdateClient::enveloped404IsNotAnOldDaemon()
{
    FakeDaemon daemon;
    daemon.token = makeToken('6');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //no pending update for it: 404 package_not_found, with an envelope
    m_mgr->upgradePackage(QStringLiteral("calaos-home"));
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("error"));
    QCOMPARE(m_mgr->get_lastErrorString(), QStringLiteral("the package has no pending update"));
    QCOMPARE(daemon.count("POST", "/api/v2/update/upgrade"), 1);
    QCOMPARE(daemon.countPrefix(QStringLiteral("/api/update/")), 0);
    QCOMPARE(m_api->apiVersion(), CalaosOsProtocol::ApiVersion::V2);
}

void TstUpdateClient::v2DaemonWithoutJobsUpgradesOnLegacyRoute()
{
    FakeDaemon daemon;
    daemon.token = makeToken('7');
    daemon.hasJobs = false;
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    m_mgr->upgradeAll();
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    QCOMPARE(daemon.count("POST", "/api/update/upgrade-all"), 1);
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QCOMPARE(m_api->apiVersion(), CalaosOsProtocol::ApiVersion::V2);
}

void TstUpdateClient::unauthorizedRereadsTokenAndRetriesOnce()
{
    FakeDaemon daemon;
    daemon.token = makeToken('8');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //the token file is rewritten while the kiosk runs
    daemon.token = makeToken('9');
    writeToken(daemon.token);

    m_mgr->refresh();
    QTRY_COMPARE(daemon.count("GET", "/api/v2/update/check"), 2);
    QTRY_VERIFY(!m_mgr->get_isChecking());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
    QVERIFY(m_mgr->get_daemonAvailable());

    QList<QByteArray> auths;
    for (const HttpRequest &r: std::as_const(daemon.requests))
        if (r.path == QStringLiteral("/api/v2/update/check"))
            auths << r.authorization;
    QCOMPARE(auths.at(0), QByteArray("Bearer ") + makeToken('8').toUtf8());
    QCOMPARE(auths.at(1), QByteArray("Bearer ") + makeToken('9').toUtf8());
}

void TstUpdateClient::unauthorizedTwiceFails()
{
    FakeDaemon daemon;
    daemon.token = makeToken('a') + QStringLiteral("x");
    writeToken(makeToken('0'));
    m_tokens << daemon.token;
    QVERIFY(daemon.start());
    startClient(daemon);

    //wrong token in the file: one retry per call, then failure
    QTRY_VERIFY(daemon.count("GET", "/api/v2/update/available") >= 2);
    QTest::qWait(200);
    QCOMPARE(daemon.count("GET", "/api/v2/update/available"), 2);
    QVERIFY(!m_mgr->get_daemonAvailable());
    //a 401 is not a sign of an old daemon
    QVERIFY(m_api->apiVersion() != CalaosOsProtocol::ApiVersion::Legacy);
    QCOMPARE(daemon.countPrefix(QStringLiteral("/api/update/")), 0);
}

void TstUpdateClient::upgradeInProgressAdoptsTheRunningJob()
{
    FakeDaemon daemon;
    daemon.token = makeToken('b');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //a run we never heard of (its job event was lost) holds the lock
    daemon.job = QJsonObject{{ "id", "00000000cafe0001" }, { "kind", "upgrade_all" }, { "status", "running" },
                             { "reboot_required", false }, { "daemon_restart", false }};
    daemon.busy = true;
    daemon.busyJobId = QStringLiteral("00000000cafe0001");

    m_mgr->upgradeAll();
    QTRY_COMPARE(daemon.count("POST", "/api/v2/update/upgrade-all"), 1);
    QTest::qWait(300);
    QVERIFY(m_mgr->get_isUpgrading());

    daemon.finishJob(QStringLiteral("done"), false);
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("idle"));
}

void TstUpdateClient::upgradeInProgressWithoutJobIdFails()
{
    FakeDaemon daemon;
    daemon.token = makeToken('c');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    //the lock of a self-upgrade carries no job id
    daemon.busy = true;
    m_mgr->upgradeAll();
    QTRY_VERIFY(!m_mgr->get_isUpgrading());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("error"));
    QVERIFY(!m_mgr->get_lastErrorString().isEmpty());
}

void TstUpdateClient::checkDuringUpgradeKeepsDaemonAvailable()
{
    FakeDaemon daemon;
    daemon.token = makeToken('d');
    writeToken(daemon.token);
    QVERIFY(daemon.start());
    startClient(daemon);
    QTRY_COMPARE(daemon.clients.size(), 1);
    QTRY_COMPARE(m_mgr->get_updatesCount(), 1);

    daemon.busy = true;
    m_mgr->refresh();
    QTRY_COMPARE(daemon.count("GET", "/api/v2/update/check"), 1);
    QTRY_VERIFY(!m_mgr->get_isChecking());
    QCOMPARE(m_mgr->get_updateState(), QStringLiteral("error"));
    QVERIFY(m_mgr->get_daemonAvailable());
}

void TstUpdateClient::rebootDuringUpgradeIsRefused_data()
{
    QTest::addColumn<bool>("hasV2");
    QTest::newRow("v2") << true;
    QTest::newRow("legacy") << false;
}

void TstUpdateClient::rebootDuringUpgradeIsRefused()
{
    QFETCH(bool, hasV2);

    FakeDaemon daemon;
    daemon.token = makeToken('e');
    daemon.hasV2 = hasV2;
    writeToken(daemon.token);
    QVERIFY(daemon.start());

    CalaosOsAPI api;
    api.setBaseAddr(daemon.baseAddr());
    api.setTokenFile(m_tokenPath);

    daemon.busy = true;
    daemon.busyJobId = QStringLiteral("00000000cafe0002");
    bool done = false;
    CalaosOsReply reply;
    api.rebootMachine([&](const CalaosOsReply &r) { reply = r; done = true; });
    QTRY_VERIFY(done);
    QVERIFY(!reply.ok);
    QCOMPARE(reply.httpStatus, 409);
    QVERIFY(reply.upgradeInProgress);
    QCOMPARE(daemon.reboots, 0);

    daemon.busy = false;
    done = false;
    api.rebootMachine([&](const CalaosOsReply &r) { reply = r; done = true; });
    QTRY_VERIFY(done);
    QVERIFY(reply.ok);
    QVERIFY(!reply.upgradeInProgress);
    QCOMPARE(daemon.reboots, 1);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstUpdateClient tc;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_updateclient.moc"
