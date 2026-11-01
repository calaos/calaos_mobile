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
 *  - a single event the server never answers stops loading after the
 *    guard timeout (the push view spinner used to run forever);
 *  - logout and loginFailed reset the state (Application::logout() and
 *    loginFailed() call reset()): after a logout, another home shows none
 *    of the previous rows and pages from 0;
 *  - loadMore() stops at the last page, a short last page is not loaded
 *    twice and an event already shown is not added again;
 *  - the view asking for more while a refresh replaces the rows (found on
 *    the simulator) does not start a request with stale paging;
 *  - identical consecutive events are one row with a count, also across two
 *    pages; events that differ, are not consecutive, or carry a picture are
 *    kept apart, and paging still follows the events the server sent;
 *  - QML sees loading, loadingMore and error change once per operation, and
 *    never an idle model in the middle of a refresh or before a page's rows.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>
#include <functional>

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
//has the id "ev-<i>" (the uuid, sent as "id" like HistEvent::toJson) and
//the message "<label> #<i>". Paged like
//HistLogger::getEvents(): total_page rounded up, a page past it is an error.
struct FakeLog
{
    int totalCount = 120;
    QString label = QStringLiteral("a");
    //Rewrites event i of a page before it is sent.
    std::function<void(int, QJsonObject &)> shape;

    static void setRaw(QJsonObject &event, const QString &message, const QString &picUid = QString())
    {
        QJsonObject raw{{ "message", message }};
        if (!picUid.isEmpty())
            raw["pic_uid"] = picUid;
        event["event_raw"] = raw;
    }

    static QJsonObject event(int i, const QString &message)
    {
        return QJsonObject{
            { "id", QStringLiteral("ev-%1").arg(i) },
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
        {
            QJsonObject e = event(i, QStringLiteral("%1 #%2").arg(label).arg(i));
            if (shape)
                shape(i, e);
            events.append(e);
        }

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

using EventChange = void (*)(EventLogItem &);
Q_DECLARE_METATYPE(EventChange)

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

    int count(int row) const
    {
        EventLogItem *it = dynamic_cast<EventLogItem *>(model->item(row));
        return it ? it->get_evCount() : 0;
    }

    //Logs in and waits for the first page, whatever its rows are.
    void loginAndSettle(FakeCalaosWs &ws)
    {
        con->login(TestUser, TestPass, ws.url());
        QTRY_COMPARE(homes, 1);
        QTRY_VERIFY(ws.pageRequests() == 1 && !model->isLoading());
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
        //Application::loginFailed() calls eventLogModel->reset().
        connect(con, &CalaosConnection::loginFailed, model, &EventLogModel::reset);
    }

    //Application::logout(): the connection, then the history.
    //Mirrors Application::logout() and the loginFailed handler (src/Application.cpp):
    //Application cannot be linked in this harness, so these tests check the model and
    //connection behaviour, not that Application still calls eventLogModel->reset().
    void appLogout()
    {
        con->logout();
        model->reset();
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
    void singleEventReleasedByTimeout();
    void logoutResetsState();
    void logoutThenAnotherHome();
    void refusedWebsocketLoginFailsOnce();
    void loginFailedResetsState();
    void loadMoreStopsAtLastPage();
    void loadMoreSkipsEventsAlreadyShown();
    void viewLoadingMoreWhileRowsAreReplaced();
    void identicalEventsShareOneRow();
    void refreshCountsFromScratch();
    void identicalEventsAcrossTwoPages();
    void eventsNotConsecutiveStayApart();
    void eventAlreadyShownIsNotCounted();
    void picturesAreNeverGrouped();
    void anotherDayIsNotGrouped();
    void recentDaysAreNamed();
    void countReachesQmlThroughGetEventModel();
    void sameEventComparesWhatTheRowShows_data();
    void sameEventComparesWhatTheRowShows();
    void loadingFollowsARequest();
    void refreshNeverLooksIdle();
    void pageRowsAreInWhenLoadingEnds();
    void errorAnswerIsShownUntilTheNextRequest();
    void silentServerIsAnError();
    void connectionLostInFlightIsAnError();
    void connectionLostIdleIsNoError();
    void noSessionIsAnError();
    void resetClearsTheError();
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

//A single event (push notification view) the server never answers: the
//item stops loading after the guard timeout, the connection forgets the
//request, and a late answer changes nothing.
void TstEventLogSession::singleEventReleasedByTimeout()
{
    model->setRequestTimeout(300);
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    QScopedPointer<QObject> obj(model->loadEvent(QStringLiteral("ev-3")));
    EventLogItem *item = qobject_cast<EventLogItem *>(obj.data());
    QVERIFY(item);
    QTRY_COMPARE(ws.requests.size(), 2);
    QVERIFY(item->get_loading());
    QCOMPARE(con->pendingEventLogRequests(), 1);

    QTRY_VERIFY(!item->get_loading());
    QCOMPARE(con->pendingEventLogRequests(), 0);

    ws.answer(1);
    QTest::qWait(200);
    QCOMPARE(item->get_evNotifText(), QString());

    //A new item still loads once the server answers.
    ws.mode = FakeCalaosWs::Answer;
    QScopedPointer<QObject> obj2(model->loadEvent(QStringLiteral("ev-4")));
    EventLogItem *item2 = qobject_cast<EventLogItem *>(obj2.data());
    QVERIFY(item2);
    QTRY_COMPARE(item2->get_evNotifText(), QStringLiteral("single ev-4"));
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

//Application::logout() then a login to another home: none of the rows of the
//first home stay, and the paging starts over (it used to continue from the
//page count of the first home).
void TstEventLogSession::logoutThenAnotherHome()
{
    FakeCalaosWs home1;
    QVERIFY(home1.start());
    home1.log.label = QStringLiteral("home1");
    loginAndLoad(home1);
    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
    QTRY_VERIFY(!model->isLoading());

    appLogout();
    QCOMPARE(model->rowCount(), 0);
    QVERIFY(!model->isLoading());
    QVERIFY(!model->isAtEnd());

    FakeCalaosWs home2;
    QVERIFY(home2.start());
    home2.log.label = QStringLiteral("home2");
    home2.log.totalCount = 80;
    con->login(TestUser, TestPass, home2.url());
    QTRY_COMPARE(homes, 2);
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, home2.log.label));
    QCOMPARE(home2.pageRequests(), 1);
    QCOMPARE(home2.requests.at(0).data["page"].toInt(), 0);

    model->loadMore();
    QTRY_COMPARE(home2.pageRequests(), 2);
    QCOMPARE(home2.requests.at(1).data["page"].toInt(), 1);
    QTRY_VERIFY(rowsAre(80, home2.log.label));
    QCOMPARE(home1.pageRequests(), 2);
}

//A login refused over the WebSocket, the transport of a mobile login: one
//attempt, one loginFailed, no fallback to HTTP.
void TstEventLogSession::refusedWebsocketLoginFailsOnce()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.acceptLogins = false;
    QSignalSpy failed(con, &CalaosConnection::loginFailed);
    QSignalSpy states(con, &CalaosConnection::connectionStateChanged);

    con->login(TestUser, TestPass, ws.url());
    QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 8000);
    QCOMPARE(con->connectionState(), ReconnectPolicy::Disconnected);
    for (const QList<QVariant> &state: std::as_const(states))
        QVERIFY(state.at(0).toInt() != ReconnectPolicy::Reconnecting);
}

//The session drops with a page in flight and the server then rejects the
//credentials until the client gives up (loginFailed, on which Application
//resets the history): the rows and the paging of that session are
//forgotten, and the next login loads from page 0.
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

//EventLogView.qml calls loadMore() when the list reaches its end, and
//clear() puts it there: while a refresh replaces the rows, the view asks for
//more. Seen on the simulator: with the request already marked finished, that
//asked for the next page of the *previous* paging in the middle of the
//refresh. The request must count as in flight until its rows are in.
void TstEventLogSession::viewLoadingMoreWhileRowsAreReplaced()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 200;
    loginAndLoad(ws);
    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 2 * DEFAULT_PER_PAGE);
    model->loadMore();
    QTRY_COMPARE(model->rowCount(), 3 * DEFAULT_PER_PAGE);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(ws.pageRequests(), 3);

    //The view: the list emptied by clear() is at its end.
    int viewCalls = 0;
    connect(model, &QAbstractItemModel::modelReset, this, [this, &viewCalls]()
    {
        viewCalls++;
        model->loadMore();
    });

    ws.log.label = QStringLiteral("refreshed");
    model->refresh();
    QTRY_VERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(viewCalls > 0);
    QTest::qWait(200);
    QCOMPARE(ws.pageRequests(), 4); //the refresh only
    QCOMPARE(ws.requests.last().data["page"].toInt(), 0);
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label));

    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 5);
    QCOMPARE(ws.requests.last().data["page"].toInt(), 1);
    QTRY_VERIFY(rowsAre(2 * DEFAULT_PER_PAGE, ws.log.label));
}

//Five events in a row with a second between them, then events that all
//differ. The row keeps the time of the newest one, the first sent.
void TstEventLogSession::identicalEventsShareOneRow()
{
    const QDateTime newest = QDateTime(QDate(2026, 1, 15), QTime(12, 0, 30), Qt::UTC);
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 8;
    ws.log.shape = [newest](int i, QJsonObject &e)
    {
        if (i < 5)
            FakeLog::setRaw(e, QStringLiteral("Clim Buanderie"));
        e["created_at"] = newest.addSecs(-i).toString("yyyy-MM-dd hh:mm:ss");
    };
    loginAndSettle(ws);

    QCOMPARE(model->rowCount(), 4);
    QCOMPARE(message(0), QStringLiteral("Clim Buanderie"));
    QCOMPARE(count(0), 5);
    QCOMPARE(model->item(0)->data(EventLogModel::RoleCount).toInt(), 5);
    const auto *first = dynamic_cast<EventLogItem *>(model->item(0));
    QCOMPARE(first->get_evTime(), newest.toLocalTime().time().toString("hh:mm:ss"));
    for (int row = 1; row < 4; row++)
    {
        QCOMPARE(message(row), QStringLiteral("a #%1").arg(row + 4));
        QCOMPARE(count(row), 1);
    }
}

void TstEventLogSession::refreshCountsFromScratch()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 3;
    ws.log.shape = [](int, QJsonObject &e) { FakeLog::setRaw(e, QStringLiteral("same")); };
    loginAndSettle(ws);
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(count(0), 3);

    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 2);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(count(0), 3);
}

//The run starts on page 0 and ends on page 1: the last row of the list grows,
//no row is added for it, and page 2 is still the next one asked for even
//though the rows do not add up to two pages.
void TstEventLogSession::identicalEventsAcrossTwoPages()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.shape = [](int i, QJsonObject &e)
    {
        if (i >= 45 && i < 55)
            FakeLog::setRaw(e, QStringLiteral("same"));
    };
    loginAndSettle(ws);
    QCOMPARE(model->rowCount(), 46);
    QCOMPARE(count(45), 5);

    QSignalSpy changed(model, &QAbstractItemModel::dataChanged);
    model->loadMore();
    QTRY_VERIFY(ws.pageRequests() == 2 && !model->isLoading());
    QCOMPARE(model->rowCount(), 91);
    QCOMPARE(message(45), QStringLiteral("same"));
    QCOMPARE(count(45), 10);
    QCOMPARE(message(46), QStringLiteral("a #55"));

    //The delegate of the row follows the count.
    bool rowNotified = false;
    for (const QList<QVariant> &args: changed)
        rowNotified = rowNotified || args.at(0).toModelIndex().row() == 45;
    QVERIFY(rowNotified);

    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QCOMPARE(ws.requests.last().data["page"].toInt(), 2);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(model->rowCount(), 111);
    QVERIFY(model->isAtEnd());
}

void TstEventLogSession::eventsNotConsecutiveStayApart()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 4;
    ws.log.shape = [](int i, QJsonObject &e)
    {
        FakeLog::setRaw(e, i == 2 ? QStringLiteral("B") : QStringLiteral("A"));
    };
    loginAndSettle(ws);

    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(message(0), QStringLiteral("A"));
    QCOMPARE(count(0), 2);
    QCOMPARE(message(1), QStringLiteral("B"));
    QCOMPARE(count(1), 1);
    QCOMPARE(message(2), QStringLiteral("A"));
    QCOMPARE(count(2), 1);
}

//New events logged between two pages shift them: the event sent again must
//not count twice.
void TstEventLogSession::eventAlreadyShownIsNotCounted()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.shape = [](int i, QJsonObject &e)
    {
        if (i >= 49 && i < 53)
            FakeLog::setRaw(e, QStringLiteral("same"));
        if (i >= DEFAULT_PER_PAGE)
            e["id"] = QStringLiteral("ev-%1").arg(i - 1);
    };
    loginAndSettle(ws);
    QCOMPARE(count(49), 1);

    model->loadMore();
    QTRY_VERIFY(ws.pageRequests() == 2 && !model->isLoading());
    QCOMPARE(count(49), 3);
}

//The first and last two carry a picture, the two in the middle are plain.
void TstEventLogSession::picturesAreNeverGrouped()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 6;
    ws.log.shape = [](int i, QJsonObject &e)
    {
        const bool picture = i < 2 || i >= 4;
        FakeLog::setRaw(e, QStringLiteral("same"),
                        picture ? QStringLiteral("pic-%1").arg(i) : QString());
    };
    loginAndSettle(ws);

    QCOMPARE(model->rowCount(), 5);
    const QList<int> counts{ 1, 1, 2, 1, 1 };
    for (int row = 0; row < counts.size(); row++)
        QCOMPARE(count(row), counts.at(row));
}

void TstEventLogSession::anotherDayIsNotGrouped()
{
    const QDateTime day = QDateTime(QDate(2026, 1, 15), QTime(12, 0, 0), Qt::UTC);
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 4;
    ws.log.shape = [day](int i, QJsonObject &e)
    {
        FakeLog::setRaw(e, QStringLiteral("same"));
        e["created_at"] = day.addDays(i < 2 ? 0 : -1).toString("yyyy-MM-dd hh:mm:ss");
    };
    loginAndSettle(ws);

    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(count(0), 2);
    QCOMPARE(count(1), 2);
}

//Noon stays on the same local day once written in UTC, as the server stores it.
void TstEventLogSession::recentDaysAreNamed()
{
    const QDateTime noon(QDate::currentDate(), QTime(12, 0, 0));
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 3;
    ws.log.shape = [noon](int i, QJsonObject &e)
    {
        e["created_at"] = noon.addDays(-i).toUTC().toString("yyyy-MM-dd hh:mm:ss");
    };
    loginAndSettle(ws);

    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->item(0)->data(EventLogModel::RoleDate).toString(), QStringLiteral("Today"));
    QCOMPARE(model->item(1)->data(EventLogModel::RoleDate).toString(), QStringLiteral("Yesterday"));
    QCOMPARE(model->item(2)->data(EventLogModel::RoleDate).toString(),
             noon.date().addDays(-2).toString(QLocale::system().dateFormat(QLocale::ShortFormat)));
}

//QML reads a row by getEventModel() and by the evCount role.
void TstEventLogSession::countReachesQmlThroughGetEventModel()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    ws.log.totalCount = 5;
    ws.log.shape = [](int i, QJsonObject &e)
    {
        if (i < 4)
            FakeLog::setRaw(e, QStringLiteral("same"));
    };
    loginAndSettle(ws);

    QQmlApplicationEngine engine;
    EventLogModel withEngine(&engine, con);
    withEngine.refresh();
    QTRY_COMPARE(withEngine.rowCount(), 2);
    QTRY_VERIFY(!withEngine.isLoading());

    QVERIFY(withEngine.roleNames().key("evCount", -1) == EventLogModel::RoleCount);

    QObject *group = withEngine.getEventModel(0);
    QVERIFY(group);
    QCOMPARE(group->property("evCount").toInt(), 4);
    QCOMPARE(QQmlEngine::objectOwnership(group), QQmlEngine::CppOwnership);

    //Rows, not events: the row after the group is the next event shown.
    QObject *next = withEngine.getEventModel(1);
    QVERIFY(next);
    QCOMPARE(next->property("evNotifText").toString(), QStringLiteral("a #4"));
    QCOMPARE(next->property("evCount").toInt(), 1);
    QCOMPARE(withEngine.getEventModel(2), nullptr);
}

void TstEventLogSession::sameEventComparesWhatTheRowShows_data()
{
    QTest::addColumn<EventChange>("change");
    QTest::addColumn<bool>("same");

    QTest::newRow("nothing") << EventChange([](EventLogItem &) {}) << true;
    QTest::newRow("time") << EventChange([](EventLogItem &e) { e.update_evTime("11:59:59"); }) << true;
    QTest::newRow("type") << EventChange([](EventLogItem &e) { e.update_evType(Common::EventIoChanged); }) << false;
    QTest::newRow("title") << EventChange([](EventLogItem &e) { e.update_evTitle("other"); }) << false;
    QTest::newRow("room") << EventChange([](EventLogItem &e) { e.update_evRoomName("other"); }) << false;
    QTest::newRow("action") << EventChange([](EventLogItem &e) { e.update_evActionText("other"); }) << false;
    QTest::newRow("notification") << EventChange([](EventLogItem &e) { e.update_evNotifText("other"); }) << false;
    QTest::newRow("date") << EventChange([](EventLogItem &e) { e.update_evDate("other"); }) << false;
    QTest::newRow("picture") << EventChange([](EventLogItem &e) { e.update_evHasPicture(true); }) << false;
}

void TstEventLogSession::sameEventComparesWhatTheRowShows()
{
    QFETCH(EventChange, change);
    QFETCH(bool, same);

    const auto fill = [](EventLogItem &e)
    {
        e.update_evType(Common::EventPush);
        e.update_evTitle("Clim Buanderie");
        e.update_evRoomName("Buanderie");
        e.update_evActionText("remote temperature");
        e.update_evNotifText("");
        e.update_evDate("Today");
        e.update_evTime("12:00:00");
        e.update_evHasPicture(false);
    };
    EventLogItem a(nullptr, con);
    EventLogItem b(nullptr, con);
    fill(a);
    fill(b);
    change(b);

    QCOMPARE(a.isSameEvent(b), same);
    QCOMPARE(b.isSameEvent(a), same);
}

void TstEventLogSession::loadingFollowsARequest()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);
    QVERIFY(!model->isLoadingMore());

    QSignalSpy loading(model, &EventLogModel::loadingChanged);
    ws.mode = FakeCalaosWs::Hold;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 2);
    QVERIFY(model->isLoading());
    QVERIFY(model->isLoadingMore());
    QCOMPARE(loading.count(), 1);

    ws.answer(1);
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(!model->isLoadingMore());
    QCOMPARE(loading.count(), 2);

    //A refresh shows no "earlier events" line: it does not append.
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QVERIFY(model->isLoading());
    QVERIFY(!model->isLoadingMore());
    QCOMPARE(loading.count(), 3);
    ws.answer(2);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(loading.count(), 4);
}

//The view reacts to loading going down by asking for the next page: a refresh
//replacing the one in flight must not look like the end of a request.
void TstEventLogSession::refreshNeverLooksIdle()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->loadMore();
    QTRY_COMPARE(ws.pageRequests(), 2);

    QList<bool> seen;
    connect(model, &EventLogModel::loadingChanged, this, [this, &seen]()
    {
        seen << model->isLoading();
    });

    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 3);
    QVERIFY(model->isLoading());
    QVERIFY(!model->isLoadingMore());
    QCOMPARE(seen, QList<bool>{ true }); //loadingMore went down, loading did not

    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 4);
    QCOMPARE(seen, QList<bool>{ true });

    ws.answer(3);
    QTRY_VERIFY(!model->isLoading());
    QCOMPARE(seen, (QList<bool>{ true, false }));
}

//Same reason: the rows must be in when the view looks at where the list ends.
void TstEventLogSession::pageRowsAreInWhenLoadingEnds()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    QList<int> rowsSeen;
    connect(model, &EventLogModel::loadingChanged, this, [this, &rowsSeen]()
    {
        if (!model->isLoading())
            rowsSeen << model->rowCount();
    });
    model->loadMore();
    QTRY_VERIFY(!rowsSeen.isEmpty());
    QCOMPARE(rowsSeen, QList<int>{ 2 * DEFAULT_PER_PAGE });
}

void TstEventLogSession::errorAnswerIsShownUntilTheNextRequest()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);
    QVERIFY(!model->hasError());

    QSignalSpy errors(model, &EventLogModel::errorChanged);
    ws.mode = FakeCalaosWs::Error;
    model->refresh();
    QTRY_VERIFY(model->hasError());
    QVERIFY(!model->isLoading());
    QCOMPARE(errors.count(), 1);
    QVERIFY(rowsAre(DEFAULT_PER_PAGE, ws.log.label)); //kept

    //Retry: the spinner, not the error, while it runs.
    ws.mode = FakeCalaosWs::Hold;
    model->refresh();
    QVERIFY(!model->hasError());
    QVERIFY(model->isLoading());
    QCOMPARE(errors.count(), 2);

    QTRY_COMPARE(ws.pageRequests(), 3);
    ws.answerError(2);
    QTRY_VERIFY(model->hasError());
    ws.mode = FakeCalaosWs::Answer;
    model->refresh();
    QTRY_VERIFY(!model->isLoading());
    QVERIFY(!model->hasError());
}

void TstEventLogSession::silentServerIsAnError()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    model->setRequestTimeout(100);
    ws.mode = FakeCalaosWs::Hold;
    QSignalSpy loading(model, &EventLogModel::loadingChanged);
    model->refresh();
    QVERIFY(!model->hasError());
    QTRY_VERIFY(model->hasError());
    QVERIFY(!model->isLoading());
    QCOMPARE(loading.count(), 2);
}

void TstEventLogSession::connectionLostInFlightIsAnError()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    ws.mode = FakeCalaosWs::Hold;
    model->refresh();
    QTRY_COMPARE(ws.pageRequests(), 2);
    QVERIFY(!model->hasError());

    ws.stop();
    QTRY_VERIFY(model->hasError());
    QVERIFY(!model->isLoading());
}

//Nothing was asked for: the refresh after the reconnection has nothing to
//recover from.
void TstEventLogSession::connectionLostIdleIsNoError()
{
    FakeCalaosWs ws;
    QVERIFY(ws.start());
    loginAndLoad(ws);

    QSignalSpy errors(model, &EventLogModel::errorChanged);
    con->logout();
    QTest::qWait(50);
    QVERIFY(!model->hasError());
    QCOMPARE(errors.count(), 0);
}

//Retry with the connection down: no spinner, the error stays.
void TstEventLogSession::noSessionIsAnError()
{
    QSignalSpy loading(model, &EventLogModel::loadingChanged);
    model->refresh();
    QVERIFY(model->hasError());
    QVERIFY(!model->isLoading());
    QCOMPARE(loading.count(), 0);

    model->loadMore();
    QVERIFY(model->hasError());
    QVERIFY(!model->isLoading());
}

//The view asks for more when clear() empties the list, without a session.
void TstEventLogSession::resetClearsTheError()
{
    model->refresh();
    QVERIFY(model->hasError());

    connect(model, &QAbstractItemModel::modelReset, this, [this]() { model->loadMore(); });
    QSignalSpy loading(model, &EventLogModel::loadingChanged);
    model->reset();
    QVERIFY(!model->hasError());
    QVERIFY(!model->isLoading());
    QCOMPARE(loading.count(), 0);
}

QTEST_MAIN(TstEventLogSession)
#include "tst_eventlogsession.moc"
