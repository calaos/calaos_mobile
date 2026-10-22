/*
 * Event log over a real session (EVLOG-1).
 *
 * Reported on iOS: after calaos_server restarted (box update), the event
 * history stayed frozen on the events of the day before until the app was
 * killed. EventLogModel had a one way "loading" latch: set by every page
 * request, cleared only by a successful page. Any request whose answer never
 * came (transport torn down, server error, no session, HTTP answer decoded
 * from the wrong place) left it set, and refresh(), pull-to-refresh and
 * loadMore() all did nothing from then on.
 *
 * These cases drive a real CalaosConnection and a real EventLogModel, wired
 * like Application (homeLoaded -> refresh()), against a fake calaos_server
 * that echoes the msg_id of each eventlog request like the real one
 * (JsonApiHandlerWS::processEventLog), and count what the server receives:
 *
 *  - a page in flight when the server restarts: the refresh after the
 *    reconnection is sent and the rows are replaced;
 *  - iOS: a page in flight when the app goes to the background
 *    (pause()/unpause() + login): reloaded when it comes back;
 *  - an {"error": ...} answer ends the request, the next refresh is sent;
 *  - refresh()/loadMore() with no session up: nothing blocks the next one;
 *  - a server that never answers: the guard timeout lets the next request go;
 *  - HTTP transport: the top level page api.php sends loads;
 *  - a late answer to an abandoned request, and the answer to a single event
 *    (push notification view), never end nor corrupt the page in flight, and
 *    a page answer is never taken for a single event;
 *  - logout and loginFailed reset the state;
 *  - loadMore() stops at the last page, a short last page is not loaded
 *    twice and an event already shown is not added again.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include "CalaosConnection.h"
#include "EventLogModel.h"
#include "RoomModel.h"

//EventLogItem::load() resolves the io of an event through IOCache. Linking
//RoomModel.cpp would drag quickflux in: no io is ever known here.
IOBase *IOCache::searchInput(QString) { return nullptr; }
IOBase *IOCache::searchOutput(QString) { return nullptr; }

static const QString TestUser = QStringLiteral("user");
static const QString TestPass = QStringLiteral("pass");

//The event log of the fake server: totalCount events, newest first, event i
//has the uuid "ev-<i>" and the message "<label> #<i>". Paged like
//HistLogger::getEvents(): total_page rounded up, a page past it is an error.
struct FakeLog
{
    int totalCount = 120;
    QString label = QStringLiteral("a");

    static QJsonObject event(int i, const QString &message)
    {
        return QJsonObject{
            { "uuid", QStringLiteral("ev-%1").arg(i) },
            { "event_type", "22" },
            { "created_at", QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd hh:mm:ss") },
            { "io_id", "" },
            { "io_state", "" },
            { "pic_uid", "" },
            { "event_raw", QJsonObject{{ "message", message }} },
        };
    }

    QJsonObject page(int page, int perPage) const
    {
        const int totalPage = totalCount / perPage + (totalCount % perPage ? 1 : 0);
        if (page < 0 || page > totalPage)
            return QJsonObject{{ "error", "page is out of range" }};

        QJsonArray events;
        for (int i = page * perPage; i < qMin(totalCount, (page + 1) * perPage); i++)
            events.append(event(i, QStringLiteral("%1 #%2").arg(label).arg(i)));

        return QJsonObject{
            { "total_page", totalPage },
            { "total_count", totalCount },
            { "page", page },
            { "per_page", perPage },
            { "events", events },
        };
    }

    QJsonObject single(const QString &uuid) const
    {
        return event(uuid.mid(3).toInt(), QStringLiteral("single %1").arg(uuid));
    }
};

/*
 * calaos_server's websocket reduced to what the event log needs: login,
 * get_home, and eventlog answered right away (Answer), never (Hold, the test
 * answers by hand later), or with the server's error (Error). Every eventlog
 * request is recorded with its msg_id, which the answer echoes.
 */
class FakeCalaosWs: public QObject
{
    Q_OBJECT

public:
    enum Mode { Answer, Hold, Error };

    struct Request
    {
        QPointer<QWebSocket> socket;
        QString msgId;
        QJsonObject data;
        bool isPage() const { return !data.contains("uuid"); }
    };

    Mode mode = Answer;
    bool acceptLogins = true;
    FakeLog log;
    int loginAttempts = 0;
    QList<Request> requests;

    bool start(quint16 port = 0)
    {
        if (!server.listen(QHostAddress::LocalHost, port))
            return false;
        m_port = server.serverPort();
        return true;
    }

    //calaos_server going down: every socket dropped, nothing listens.
    void stop()
    {
        const QList<QWebSocket *> socks = clients;
        clients.clear();
        for (QWebSocket *c: socks)
            c->abort();
        server.close();
    }

    QString url() const { return QStringLiteral("ws://127.0.0.1:%1/api").arg(m_port); }
    quint16 port() const { return m_port; }

    int pageRequests() const
    {
        int n = 0;
        for (const Request &r: requests)
            n += r.isPage() ? 1 : 0;
        return n;
    }

    void send(const Request &r, const QJsonObject &payload)
    {
        if (!r.socket)
            return;
        QJsonObject o{{ "msg", "eventlog" }, { "data", payload }};
        if (!r.msgId.isEmpty())
            o["msg_id"] = r.msgId;
        r.socket->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }

    //What the server answers to that request now.
    void answer(int idx)
    {
        const Request &r = requests.at(idx);
        send(r, r.isPage() ? log.page(r.data["page"].toInt(), r.data["per_page"].toInt())
                           : log.single(r.data["uuid"].toString()));
    }

    void answerError(int idx)
    {
        send(requests.at(idx), QJsonObject{{ "error", "database is locked" }});
    }

    ~FakeCalaosWs() override
    {
        stop();
    }

    FakeCalaosWs()
    {
        connect(&server, &QWebSocketServer::newConnection, this, [this]()
        {
            QWebSocket *c = server.nextPendingConnection();
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

private:
    void onMessage(QWebSocket *c, const QJsonObject &j)
    {
        const QString msg = j["msg"].toString();
        auto reply = [c](const QJsonObject &o)
        {
            c->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
        };

        if (msg == "login")
        {
            loginAttempts++;
            const QJsonObject d = j["data"].toObject();
            const bool ok = acceptLogins && d["cn_user"] == TestUser && d["cn_pass"] == TestPass;
            reply({{ "msg", "login" }, { "data", QJsonObject{{ "success", ok ? "true" : "false" }} }});
        }
        else if (msg == "get_home")
        {
            reply({{ "msg", "get_home" }, { "data", QJsonObject{{ "home", QJsonArray() }} }});
        }
        else if (msg == "eventlog")
        {
            requests << Request{ c, j["msg_id"].toString(), j["data"].toObject() };
            if (mode == Answer)
                answer(requests.size() - 1);
            else if (mode == Error)
                answerError(requests.size() - 1);
        }
    }

    QWebSocketServer server{ QStringLiteral("fake calaos_server"), QWebSocketServer::NonSecureMode };
    QList<QWebSocket *> clients;
    quint16 m_port = 0;
};

/*
 * api.php reduced to get_home (the login), poll_listen (register, then held
 * open) and eventlog, answered with the top level page the real server sends
 * (JsonApiHandlerHttp::processEventLog: no "data" around it).
 */
class FakeApiPhp: public QTcpServer
{
    Q_OBJECT

public:
    FakeLog log;
    QList<QJsonObject> eventlogRequests;

    QString url() const { return QStringLiteral("http://127.0.0.1:%1/api.php").arg(serverPort()); }

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
    void handleRequest(QTcpSocket *sock, const QJsonObject &req)
    {
        const QString action = req["action"].toString();
        QJsonObject out{{ "success", "true" }};

        if (action == "get_home")
            out = QJsonObject{{ "success", "true" }, { "home", QJsonArray() }};
        else if (action == "poll_listen")
        {
            if (req["type"] != "register")
                return; //long-poll held open
            out = QJsonObject{{ "success", "true" }, { "uuid", "poll" }};
        }
        else if (action == "eventlog")
        {
            eventlogRequests << req;
            out = log.page(req["page"].toInt(), req["per_page"].toInt());
        }

        const QByteArray b = QJsonDocument(out).toJson(QJsonDocument::Compact);
        sock->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n"
                    "Content-Length: " + QByteArray::number(b.size()) + "\r\n\r\n" + b);
        sock->flush();
        sock->disconnectFromHost();
    }
};

class TstEventLogSession: public QObject
{
    Q_OBJECT

    CalaosConnection *con = nullptr;
    EventLogModel *model = nullptr;
    int homes = 0;

    QString message(int row) const
    {
        EventLogItem *it = dynamic_cast<EventLogItem *>(model->item(row));
        return it ? it->get_evNotifText() : QString();
    }

    //Every row once, in order: "ev-0".."ev-<n-1>" with the given label.
    bool rowsAre(int count, const QString &label) const
    {
        if (model->rowCount() != count)
            return false;
        for (int i = 0; i < count; i++)
        {
            if (message(i) != QStringLiteral("%1 #%2").arg(label).arg(i))
                return false;
        }
        return true;
    }

    //Logs in and waits for the first page, answered by the server.
    void loginAndLoad(FakeCalaosWs &ws)
    {
        con->login(TestUser, TestPass, ws.url());
        QTRY_COMPARE(homes, 1);
        QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
        QVERIFY(!model->isLoading());
    }

private slots:
    void init()
    {
        homes = 0;
        con = new CalaosConnection();
        //The engine is only used to hand rows to QML (getEventModel()).
        model = new EventLogModel(nullptr, con);
        //Application::homeLoaded() calls eventLogModel->refresh().
        connect(con, &CalaosConnection::homeLoaded, model, [this]()
        {
            homes++;
            model->refresh();
        });
    }

    void cleanup()
    {
        con->logout();
        delete model;
        model = nullptr;
        delete con;
        con = nullptr;
        //Let the sockets closed above go away before the next fake server.
        QTest::qWait(50);
    }

    void serverRestartWhileInFlight();
    void backgroundWhileInFlight();
    void errorAnswerEndsRequest();
    void noSessionDoesNotBlock();
    void silentServerReleasedByTimeout();
    void httpTransportLoadsTopLevelPage();
    void lateAnswerOfAbandonedRequestIgnored();
    void singleEventAndPageAnswersKeptApart();
    void logoutResetsState();
    void loginFailedResetsState();
    void loadMoreStopsAtLastPage();
    void loadMoreSkipsEventsAlreadyShown();
};

//Path 2 of the ticket: calaos_server (or haproxy) restarts while a page is in
//flight. The answer never comes; the refresh after the reconnection must be
//sent and replace the rows.
void TstEventLogSession::serverRestartWhileInFlight()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);
    QCOMPARE(ws.pageRequests(), 1);

    ws.mode = FakeCalaosWs::Hold;
    model->refresh(); //pull-to-refresh
    QTRY_COMPARE(ws.pageRequests(), 2);

    const quint16 port = ws.port();
    ws.stop();
    QTest::qWait(300);
    ws.mode = FakeCalaosWs::Answer;
    ws.log.label = QStringLiteral("after-restart");
    QVERIFY(ws.start(port));

    //Reconnection after the backoff, then homeLoaded -> refresh().
    QTRY_VERIFY_WITH_TIMEOUT(homes == 2, 10000);
    QTRY_COMPARE(ws.pageRequests(), 3);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    QVERIFY(!model->isLoading());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    //And the user can still pull to refresh / scroll.
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 4);
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
}

//Path 1: iOS drops the session when the app resigns active (FIX-7). A page
//asked for by the login that brought the app back is lost if the app goes to
//the background again before the answer. Coming back must reload.
void TstEventLogSession::backgroundWhileInFlight()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.mode = FakeCalaosWs::Hold; //slow answer (cold database after a restart)

    con->login(TestUser, TestPass, ws.url());
    QTRY_COMPARE(ws.pageRequests(), 1);
    QVERIFY(model->isLoading());

    //applicationWillResignActive
    con->pause();
    QVERIFY(!model->isLoading());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    //The late answer to the request of the dead session goes nowhere.
    ws.answer(0);

    //applicationBecomeActive
    ws.mode = FakeCalaosWs::Answer;
    ws.log.label = QStringLiteral("foreground");
    con->unpause();
    con->login(TestUser, TestPass, ws.url());

    QTRY_COMPARE(homes, 2);
    QTRY_COMPARE(ws.pageRequests(), 2);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    QVERIFY(!model->isLoading());
}

//Path 3: the server answers {"error": ...} (database locked, page out of
//range). That ends the request: the next refresh is sent.
void TstEventLogSession::errorAnswerEndsRequest()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.mode = FakeCalaosWs::Error;

    con->login(TestUser, TestPass, ws.url());
    QTRY_COMPARE(ws.pageRequests(), 1);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(model->rowCount(), 0);

    ws.mode = FakeCalaosWs::Answer;
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 2);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));

    //An error on a later page keeps the rows and lets the next one go.
    ws.mode = FakeCalaosWs::Error;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    ws.mode = FakeCalaosWs::Answer;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 4);
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
}

//Path 5: refresh() / loadMore() while no session is up (the list reaching
//its end during a reconnection) send nothing and must not block the refresh
//of the next login.
void TstEventLogSession::noSessionDoesNotBlock()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());

    model->refresh();
    model->loadMore();
    QVERIFY(!model->isLoading());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    QJsonObject d{{ "page", 0 }};
    QVERIFY(!con->sendJson(QStringLiteral("eventlog"), d));
    QVERIFY(con->requestEventLogPage(0, DEFAULT_PER_PAGE).isEmpty());

    loginAndLoad(ws);
    QCOMPARE(ws.pageRequests(), 1);
}

//Path 7: a server that never answers. The guard timeout abandons the request
//so the next one can go (loadMore() is refused while a page is in flight,
//refresh() is not, hence loadMore() here).
void TstEventLogSession::silentServerReleasedByTimeout()
{
    QCOMPARE(model->requestTimeout(), int(EventLogModel::DefaultRequestTimeoutMs));
    model->setRequestTimeout(300);

    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 2);
    model->loadMore(); //refused: a page is in flight
    QTest::qWait(100);
    QCOMPARE(ws.pageRequests(), 2);

    QTRY_VERIFY(!model->isLoading()); //the timeout let go
    QCOMPARE(con->pendingEventLogRequests(), 0);

    ws.mode = FakeCalaosWs::Answer;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);

    //The answer to the abandoned request, if it ever comes, changes nothing.
    ws.answer(1);
    QTest::qWait(200);
    QCOMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
}

//Path 4: over HTTP, api.php sends the page at the top level. It used to be
//read from "data" (absent), so the history never loaded.
void TstEventLogSession::httpTransportLoadsTopLevelPage()
{
    FakeApiPhp http;
    QVERIFY(http.listen(QHostAddress::LocalHost));

    con->login(TestUser, TestPass, http.url());
    QTRY_COMPARE(homes, 1);
    QTRY_COMPARE(http.eventlogRequests.size(), 1);
    QCOMPARE(http.eventlogRequests.at(0)["page"].toInt(), 0);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, http.log.label));
    QVERIFY(!model->isLoading());

    model->loadMore();
    QTRY_COMPARE(http.eventlogRequests.size(), 2);
    QCOMPARE(http.eventlogRequests.at(1)["page"].toInt(), 1);
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);

    //A refresh replaces the rows.
    http.log.label = QStringLiteral("http-refresh");
    model->refresh();
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, http.log.label));
    QCOMPARE(con->pendingEventLogRequests(), 0);
}

//refresh() starts over even when a page is in flight; the late answer to the
//abandoned request must neither end the new one nor touch the rows.
void TstEventLogSession::lateAnswerOfAbandonedRequestIgnored()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->loadMore();                  //request 1: page 1, appended
    QTRY_COMPARE(ws.pageRequests(), 2);
    model->refresh();                   //request 2: page 0, replaces
    QTRY_COMPARE(ws.pageRequests(), 3);

    //Each request has its own msg_id, the server echoes it.
    QSet<QString> ids;
    for (const FakeCalaosWs::Request &r: ws.requests)
    {
        QVERIFY2(r.msgId.startsWith("eventlog_page:"), qPrintable(r.msgId));
        ids.insert(r.msgId);
    }
    QCOMPARE(ids.size(), ws.requests.size());

    //The abandoned page 1 answers first: ignored, the refresh still waits.
    ws.log.label = QStringLiteral("stale");
    ws.answer(1);
    QTest::qWait(200);
    QVERIFY(model->isLoading());
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, QStringLiteral("a")));

    ws.log.label = QStringLiteral("fresh");
    ws.answer(2);
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));

    //And the paging follows the refresh: the next page is page 1.
    ws.mode = FakeCalaosWs::Answer;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 4);
    QCOMPARE(ws.requests.last().data["page"].toInt(), 1);
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
}

//The push notification view loads one event by uuid (loadEvent()) through
//the same "eventlog" action. A page answer, even an error, must not be taken
//for that event, and the event answer must not end the page in flight.
void TstEventLogSession::singleEventAndPageAnswersKeptApart()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->refresh();
    QTRY_COMPARE(ws.requests.size(), 2);
    QScopedPointer<QObject> obj(model->loadEvent(QStringLiteral("ev-7")));
    EventLogItem *item = qobject_cast<EventLogItem *>(obj.data());
    QVERIFY(item);
    QTRY_COMPARE(ws.requests.size(), 3);
    QVERIFY(!ws.requests.at(2).isPage());
    QVERIFY2(ws.requests.at(2).msgId.startsWith("eventlog_event:"), qPrintable(ws.requests.at(2).msgId));
    QVERIFY(item->get_loading());

    //The page fails first: the event still waits.
    ws.answerError(1);
    QTRY_VERIFY(!model->isLoading());
    QTest::qWait(100);
    QVERIFY(item->get_loading());
    QCOMPARE(item->get_evNotifText(), QString());

    //A new page in flight, then the event answers: only the event is done.
    model->refresh();
    QTRY_COMPARE(ws.requests.size(), 4);
    ws.answer(2);
    QTRY_VERIFY(!item->get_loading());
    QCOMPARE(item->get_evNotifText(), QStringLiteral("single ev-7"));
    QVERIFY(model->isLoading());
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, QStringLiteral("a")));

    ws.log.label = QStringLiteral("b");
    ws.answer(3);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    QVERIFY(!model->isLoading());
    QCOMPARE(item->get_evNotifText(), QStringLiteral("single ev-7"));

    //An event asked for while the session drops stops loading too.
    QScopedPointer<QObject> obj2(model->loadEvent(QStringLiteral("ev-8")));
    EventLogItem *item2 = qobject_cast<EventLogItem *>(obj2.data());
    QVERIFY(item2);
    QVERIFY(item2->get_loading());
    con->logout();
    QVERIFY(!item2->get_loading());
}

//An explicit logout drops whatever is in flight: the next login loads.
void TstEventLogSession::logoutResetsState()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 2);
    QVERIFY(model->isLoading());

    con->logout();
    QVERIFY(!model->isLoading());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    ws.mode = FakeCalaosWs::Answer;
    ws.log.label = QStringLiteral("relogin");
    con->login(TestUser, TestPass, ws.url());
    QTRY_COMPARE(homes, 2);
    QTRY_COMPARE(ws.pageRequests(), 3);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
}

//The session drops with a page in flight and the server then rejects the
//credentials until the client gives up (loginFailed): the rows and the
//paging of that session are forgotten, and the next login loads from page 0.
void TstEventLogSession::loginFailedResetsState()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);
    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);

    ws.mode = FakeCalaosWs::Hold;
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 3);

    QSignalSpy failed(con, &CalaosConnection::loginFailed);
    const quint16 port = ws.port();
    ws.acceptLogins = false;
    ws.stop();
    QVERIFY(ws.start(port));
    //1 lost connection, then MaxAuthFailures rejected logins (1+2+4 s).
    QTRY_VERIFY_WITH_TIMEOUT(failed.count() == 1, 15000);

    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->isLoading());
    QVERIFY(!model->isAtEnd());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    ws.acceptLogins = true;
    ws.mode = FakeCalaosWs::Answer;
    ws.log.label = QStringLiteral("back");
    con->login(TestUser, TestPass, ws.url());
    QTRY_COMPARE(homes, 2);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    QCOMPARE(ws.requests.last().data["page"].toInt(), 0);
    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
    QCOMPARE(ws.requests.last().data["page"].toInt(), 1);
}

//80 events: page 0 has 50, page 1 the last 30. loadMore() used to ask for
//page rowCount() / 50 = 1 again and duplicate those 30 rows.
void TstEventLogSession::loadMoreStopsAtLastPage()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 80;
    loginAndLoad(ws);

    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 80);
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(model->isAtEnd());

    model->loadMore();
    model->loadMore();
    QTest::qWait(200);
    QCOMPARE(ws.pageRequests(), 2);
    QVERIFY(rowsAre(80, ws.log.label));

    //A refresh starts the paging over.
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(!model->isAtEnd());
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
}

//New events logged between two pages shift the pages: the next page then
//starts with events already shown. They are not added twice.
void TstEventLogSession::loadMoreSkipsEventsAlreadyShown()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    //Five events logged meanwhile shift the pages: what the server sends as
    //page 1 now starts with ev-45..49, already shown. (The fake numbers its
    //events from the newest, so the overlap is built by hand.)
    ws.mode = FakeCalaosWs::Hold;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 2);
    ws.log.totalCount += 5;
    QJsonObject page = ws.log.page(1, DEFAULT_PER_PAGE);
    QJsonArray evs;
    for (int i = 45; i < 95; i++)
        evs.append(FakeLog::event(i, QStringLiteral("%1 #%2").arg(ws.log.label).arg(i)));
    page["events"] = evs;
    ws.send(ws.requests.at(1), page);

    QTRY_VERIFY(!model->isLoading());
    QVERIFY(rowsAre(95, ws.log.label));
}

QTEST_MAIN(TstEventLogSession)
#include "tst_eventlogsession.moc"
