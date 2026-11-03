/*
 * Server capabilities (get_server_info) and the camera token, on a real
 * CalaosConnection against an in-process calaos_server: one port serves both
 * the WebSocket and api.php, as the real one does.
 *
 *  - capability camera_token: the picture request carries the token, nothing
 *    of cn_user / cn_pass, the token is reused then renewed before it expires;
 *  - no answer to get_server_info (an old server ignores it) or no
 *    camera_token in the list: cn_pass as before, no disconnection;
 *  - the v1 picture url is assembled by concatenation: QString::arg() takes
 *    an encoded "%2F" in the user name for a placeholder.
 *
 * The credentials below are synthetic.
 */

#include <QtTest>
#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QLoggingCategory>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include "CalaosConnection.h"

namespace {

const QString TestUser = QStringLiteral("us/er€");
const QString TestPass = QStringLiteral("p%2Faß &x#1");

struct HttpRequest
{
    QByteArray raw;    //request line, headers and body, as received
    QByteArray target; //path and query
    QJsonObject json;
    int status = 200;
};

QMap<QString, QString> queryOf(const QByteArray &target)
{
    QMap<QString, QString> q;
    const int mark = target.indexOf('?');
    if (mark < 0)
        return q;
    for (const QByteArray &pair: target.mid(mark + 1).split('&'))
    {
        const int eq = pair.indexOf('=');
        if (eq > 0)
            q.insert(QUrl::fromPercentEncoding(pair.left(eq)), QUrl::fromPercentEncoding(pair.mid(eq + 1)));
    }
    return q;
}

} // namespace

/*
 * First bytes decide: an Upgrade goes to the WebSocket server, anything else
 * is an api.php request. The real calaos_server does the same on port 5454.
 */
class FakeCalaos: public QTcpServer
{
    Q_OBJECT

public:
    enum Reply { Answer, Silent, Error };

    QJsonArray capabilities{ QStringLiteral("camera_token"), QStringLiteral("admin_relay") };
    Reply serverInfo = Answer;
    Reply cameraTokenReply = Answer;
    QString expiresIn = QStringLiteral("120");
    //get_server_info answer kept until releaseServerInfo()
    bool holdServerInfo = false;

    QStringList wsMessages;       //msg of every frame, in order
    QList<HttpRequest> requests;  //every api.php request
    QSet<QString> validTokens;
    int issued = 0;

    FakeCalaos()
    {
        ws = new QWebSocketServer(QStringLiteral("fake calaos_server"),
                                  QWebSocketServer::NonSecureMode, this);
        connect(ws, &QWebSocketServer::newConnection, this, [this]()
        {
            QWebSocket *c = ws->nextPendingConnection();
            client = c;
            connect(c, &QWebSocket::textMessageReceived, this, [this, c](const QString &m)
            {
                onFrame(c, QJsonDocument::fromJson(m.toUtf8()).object());
            });
            connect(c, &QWebSocket::disconnected, this, [this, c]()
            {
                if (client == c)
                    client = nullptr;
                c->deleteLater();
            });
        });
    }

    QString url() const
    {
        //api.php: the v1 picture url is cut from the end of this address
        return QStringLiteral("ws://127.0.0.1:%1/api.php").arg(serverPort());
    }

    int count(const QString &msg) const { return wsMessages.count(msg); }

    void releaseServerInfo()
    {
        holdServerInfo = false;
        if (client && heldServerInfo)
            send(client, heldServerInfo->first, heldServerInfo->second);
        heldServerInfo.reset();
    }

    //The server forgets every token: the next picture gets a 400.
    void revokeTokens() { validTokens.clear(); }

    //Picture answers come late, so a token can be renewed in between.
    int httpDelayMs = 0;
    //Every token is refused, as by a server that has lost its token store.
    bool refuseAll = false;
    //Status of the v1 picture (get_camera_pic) answers.
    int v1Status = 200;
    //Status of the event_picture answers.
    int eventPicStatus = 200;
    //event_picture: announce more than a picture may weigh, or never answer.
    bool eventPicHuge = false;
    bool eventPicSilent = false;
    //Any picture or cover request: how the body is sent, see sendBig().
    enum Big { NoBig, Announced, NoLength, Drip, ExactLimit, OverLimit };
    Big big = NoBig;
    //Connections the client closed before the body ended.
    int bigAborted = 0;
    //eventlog answers with an error that echoes the password it was sent.
    bool eventLogEchoesPassword = false;
    //Pictures of this camera are held until releaseHeld(), and judged then:
    //a late refusal without any clock involved.
    QString holdCam;
    int heldStatus = 0;

    void releaseHeld()
    {
        for (const Held &h: std::as_const(held))
        {
            if (!h.sock)
                continue;
            const QString tok = requests[h.index].json.value(QStringLiteral("camera_token")).toString();
            requests[h.index].status = validTokens.contains(tok) ? 200 : 400;
            heldStatus = requests[h.index].status;
            writeAnswer(h.sock, requests[h.index].status);
        }
        held.clear();
    }

    //What a picture request carried, whatever the form.
    static bool carriesCredentials(const HttpRequest &r)
    {
        return r.raw.contains("cn_pass") || r.raw.contains("cn_user") ||
               r.raw.contains("pass") || r.raw.contains("%C3%9F");
    }

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        QByteArray *buf = new QByteArray();
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(sock, &QTcpSocket::readyRead, this, [this, sock, buf, conn]()
        {
            buf->append(sock->peek(sock->bytesAvailable()));
            if (buf->toLower().contains("upgrade: websocket"))
            {
                disconnect(*conn);
                delete buf;
                ws->handleConnection(sock);
                //the handshake is already buffered: nothing will signal it again
                emit sock->readyRead();
                return;
            }
            const int hdrEnd = buf->indexOf("\r\n\r\n");
            if (hdrEnd < 0)
                return;
            int contentLength = 0;
            for (const QByteArray &line: buf->left(hdrEnd).split('\n'))
            {
                if (line.toLower().startsWith("content-length:"))
                    contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
            if (buf->size() < hdrEnd + 4 + contentLength)
                return;
            disconnect(*conn);
            sock->readAll();
            httpRequest(sock, *buf, hdrEnd);
            delete buf;
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }

private:
    void httpRequest(QTcpSocket *sock, const QByteArray &raw, int hdrEnd)
    {
        HttpRequest r;
        r.raw = raw;
        const QByteArray line = raw.left(raw.indexOf("\r\n"));
        r.target = line.split(' ').value(1);
        r.json = QJsonDocument::fromJson(raw.mid(hdrEnd + 4)).object();

        const QString action = r.json.value(QStringLiteral("action")).toString();
        if (action == QLatin1String("camera") || action == QLatin1String("get_camera_pic"))
        {
            if (r.json.contains(QStringLiteral("camera_token")))
            {
                if (refuseAll || !validTokens.contains(r.json.value(QStringLiteral("camera_token")).toString()))
                    r.status = 400;
            }
            else if (action == QLatin1String("camera") &&
                     (r.json.value(QStringLiteral("cn_user")).toString() != TestUser ||
                      r.json.value(QStringLiteral("cn_pass")).toString() != TestPass))
            {
                r.status = 401;
            }
        }
        else if (action == QLatin1String("event_picture"))
        {
            r.status = eventPicStatus;
        }
        else
        {
            r.status = 400;
        }
        if (action == QLatin1String("get_camera_pic"))
            r.status = v1Status;
        requests << r;

        if (big != NoBig && (action == QLatin1String("event_picture") || action == QLatin1String("camera") ||
                             action == QLatin1String("get_camera_pic") || action == QLatin1String("audio")))
        {
            sendBig(sock);
            return;
        }
        if (action == QLatin1String("event_picture") && (eventPicHuge || eventPicSilent))
        {
            //the socket stays open: the client must give up by itself
            if (eventPicHuge)
            {
                sock->write("HTTP/1.1 200 X\r\nContent-Type: image/jpeg\r\nContent-Length: 17825792\r\n\r\n" +
                            QByteArray(1024, 'x'));
                sock->flush();
            }
            return;
        }
        if (!holdCam.isEmpty() && r.json.value(QStringLiteral("camera_id")).toString() == holdCam)
        {
            held.append({ QPointer<QTcpSocket>(sock), int(requests.size()) - 1 });
            return;
        }
        if (httpDelayMs > 0)
            QTimer::singleShot(httpDelayMs, sock, [this, sock, status = r.status]() { writeAnswer(sock, status); });
        else
            writeAnswer(sock, r.status);
    }

    //The sockets stay open (but ExactLimit): the client has to give up by itself.
    void sendBig(QTcpSocket *sock)
    {
        constexpr qint64 Limit = 16 * 1024 * 1024;
        const QByteArray head = "HTTP/1.1 200 X\r\nContent-Type: image/jpeg\r\n";
        if (big != ExactLimit)
        {
            auto counted = std::make_shared<bool>(false);
            auto count = [this, counted]() { if (!*counted) { *counted = true; ++bigAborted; } };
            connect(sock, &QTcpSocket::disconnected, this, count);
            connect(sock, &QTcpSocket::errorOccurred, this, count);
        }
        switch (big)
        {
        case Announced:
            //only 1 KiB ever arrives: the announced length alone must refuse it
            sock->write(head + "Content-Length: 17825792\r\n\r\n" + QByteArray(1024, 'x'));
            break;
        case NoLength:
            //no Content-Length, the end of the body is the end of the connection
            sock->write(head + "Connection: close\r\n\r\n");
            {
                //fed in the test's own event loop: a paced stream leaves the client room to react
                QTimer *t = new QTimer(sock);
                t->setInterval(1);
                connect(t, &QTimer::timeout, sock, [sock]() {
                    if (sock->bytesToWrite() < 1024 * 1024)
                        sock->write(QByteArray(256 * 1024, 'x'));
                });
                t->start();
            }
            break;
        case OverLimit:
            sock->write(head + "Content-Length: " + QByteArray::number(Limit + 1) + "\r\n\r\n" + QByteArray(Limit + 1, 'x'));
            break;
        case ExactLimit:
            sock->write(head + "Content-Length: " + QByteArray::number(Limit) + "\r\nConnection: close\r\n\r\n"
                        + QByteArray(Limit, 'x'));
            sock->disconnectFromHost();
            break;
        case Drip:
        {
            //one byte every 100 ms, far below the transfer timeout
            sock->write(head + "Content-Length: 200\r\n\r\n");
            QTimer *t = new QTimer(sock);
            t->setInterval(100);
            connect(t, &QTimer::timeout, sock, [sock]() { sock->write("x", 1); sock->flush(); });
            t->start();
            break;
        }
        default:
            break;
        }
        sock->flush();
    }

    static void writeAnswer(QTcpSocket *sock, int status)
    {
        QByteArray body("refused");
        if (status == 200)
        {
            //a real (1x1) image: only a decodable body counts as an accepted picture
            QImage img(1, 1, QImage::Format_RGB32);
            img.fill(Qt::red);
            QBuffer buf(&body);
            buf.open(QIODevice::WriteOnly);
            img.save(&buf, "PNG");
        }
        QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: image/jpeg";
        out += "\r\nContent-Length: " + QByteArray::number(body.size());
        out += "\r\nConnection: close\r\n\r\n" + body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }

    static void send(QWebSocket *c, const QJsonObject &frame, const QJsonObject &data)
    {
        QJsonObject o = frame;
        o[QStringLiteral("data")] = data;
        c->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }

    void onFrame(QWebSocket *c, const QJsonObject &j)
    {
        const QString msg = j.value(QStringLiteral("msg")).toString();
        wsMessages << msg;
        const QJsonObject head{{ QStringLiteral("msg"), msg },
                               { QStringLiteral("msg_id"), j.value(QStringLiteral("msg_id")) }};

        if (msg == QLatin1String("login"))
        {
            const QJsonObject d = j.value(QStringLiteral("data")).toObject();
            const bool ok = d.value(QStringLiteral("cn_user")) == TestUser &&
                            d.value(QStringLiteral("cn_pass")) == TestPass;
            send(c, head, QJsonObject{{ QStringLiteral("success"), ok ? "true" : "false" }});
        }
        else if (msg == QLatin1String("get_home"))
        {
            send(c, head, QJsonObject{{ QStringLiteral("home"), QJsonArray() }});
        }
        else if (msg == QLatin1String("get_server_info"))
        {
            if (serverInfo == Silent)
                return;
            const QJsonObject data = serverInfo == Error
                    ? QJsonObject{{ QStringLiteral("error"), QStringLiteral("unavailable") }}
                    : QJsonObject{{ QStringLiteral("version"), QStringLiteral("test") },
                                  { QStringLiteral("protocol"), QStringLiteral("1") },
                                  { QStringLiteral("capabilities"), capabilities }};
            if (holdServerInfo)
                heldServerInfo = std::make_pair(head, data);
            else
                send(c, head, data);
        }
        else if (msg == QLatin1String("eventlog") && eventLogEchoesPassword)
        {
            send(c, head, QJsonObject{{ QStringLiteral("error"), QStringLiteral("bad request, got cn_pass ") + TestPass }});
        }
        else if (msg == QLatin1String("camera_token"))
        {
            if (cameraTokenReply == Silent)
                return;
            if (cameraTokenReply == Error)
            {
                send(c, head, QJsonObject{{ QStringLiteral("error"), QStringLiteral("scope denied") }});
                return;
            }
            const QString token = QStringLiteral("%1").arg(0xabc000 + ++issued, 64, 16, QLatin1Char('0'));
            validTokens << token;
            send(c, head, QJsonObject{{ QStringLiteral("token"), token },
                                      { QStringLiteral("expires_in"), expiresIn }});
        }
    }

    struct Held
    {
        QPointer<QTcpSocket> sock;
        int index;
    };
    QList<Held> held;

    QWebSocketServer *ws = nullptr;
    QPointer<QWebSocket> client;
    std::optional<std::pair<QJsonObject, QJsonObject>> heldServerInfo;
};

class TstCameraToken: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void tokenReplacesCnPass();
    void tokenIsReusedThenRenewedBeforeExpiry();
    void pictureWaitsForCapabilities();
    void noAnswerKeepsCnPass();
    void errorAnswerKeepsCnPass();
    void capabilityMissingKeepsCnPass();
    void refusedTokenNeverFallsBackToCnPass();
    void tokenRefusedByHttpIsForgotten();
    void refusedTokenBacksOff();
    void repeatedRefusalsSpaceOutTokenRequests();
    void acceptedPictureResetsBackoff();
    void lateRefusalKeepsRenewedToken();
    void v1ErrorNeverLogsCredentials();
    void eventPictureNeverLogsCredentials();
    void eventPictureIsFetchedWithBodyCredentials();
    void eventPictureAnnouncingTooMuchIsRefused();
    void eventPictureWithoutAnswerTimesOut();
    void eventPictureWithoutLengthIsAbortedAtTheLimit();
    void eventPictureDrippedIsCutByTheTotalTimeout();
    void cameraPictureBoundsAreEnforced();
    void audioCoverBoundsAreEnforced();
    void v1SuffixWithoutQueryKeepsCredentialsOutOfThePath();
    void eventLogServerErrorNeverLogsPassword();
    void redactionKeepsDiagnosticAndCoversEdgeCases();
    void v1SuffixFragmentDoesNotHideCredentials();
    void redactErrorAndJson();
    void v1UrlIsAssembledWithoutArg();

private:
    void login();
    //Moves the token's clock, not the wall clock.
    void advance(int ms) { fakeNowMs += ms; }
    qint64 fakeNowMs = 1000000;
    //Asks a v2 picture and waits for its outcome.
    bool picture(const QString &camid);

    FakeCalaos *server = nullptr;
    CalaosConnection *connection = nullptr;
    QSignalSpy *downloaded = nullptr;
    QSignalSpy *failed = nullptr;
};

void TstCameraToken::init()
{
    server = new FakeCalaos();
    QVERIFY(server->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
    connection->setServerInfoTimeout(300);
    connection->setCameraTokenTimeout(300);
    fakeNowMs = 1000000;
    connection->setCameraTokenClock([this]() { return fakeNowMs; });
    downloaded = new QSignalSpy(connection, &CalaosConnection::cameraPictureDownloaded);
    failed = new QSignalSpy(connection, &CalaosConnection::cameraPictureFailed);
}

void TstCameraToken::cleanup()
{
    delete downloaded;
    delete failed;
    delete connection;
    delete server;
    downloaded = failed = nullptr;
    connection = nullptr;
    server = nullptr;
}

void TstCameraToken::login()
{
    connection->login(TestUser, TestPass, server->url());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(connection->isWebsocket());
}

bool TstCameraToken::picture(const QString &camid)
{
    const int before = downloaded->count();
    const int failedBefore = failed->count();
    connection->getCameraPicture(camid);
    return QTest::qWaitFor([&]() { return downloaded->count() + failed->count() > before + failedBefore; }, 3000)
           && failed->count() == failedBefore
           && downloaded->count() == before + 1
           && downloaded->last().value(0).toString() == camid;
}

void TstCameraToken::tokenReplacesCnPass()
{
    login();
    QTRY_VERIFY(connection->serverCapabilities().has(ServerCapabilities::CameraToken));

    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(downloaded->count(), 1);
    QCOMPARE(failed->count(), 0);

    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);
    QCOMPARE(server->requests.size(), 1);
    const HttpRequest &r = server->requests.first();
    QCOMPARE(r.status, 200);
    QVERIFY(r.json.value(QStringLiteral("camera_token")).toString().size() == 64);
    QCOMPARE(r.json.value(QStringLiteral("action")).toString(), QStringLiteral("camera"));
    QCOMPARE(r.json.value(QStringLiteral("camera_id")).toString(), QStringLiteral("cam1"));
    QVERIFY2(!FakeCalaos::carriesCredentials(r), r.raw.constData());
}

void TstCameraToken::tokenIsReusedThenRenewedBeforeExpiry()
{
    //2 s of life: renewed once less than half of it (1 s) is left
    server->expiresIn = QStringLiteral("2");
    login();

    QVERIFY(picture(QStringLiteral("cam1")));
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);
    const QString first = server->requests.at(0).json.value(QStringLiteral("camera_token")).toString();
    QCOMPARE(server->requests.at(1).json.value(QStringLiteral("camera_token")).toString(), first);

    advance(1150);
    //the old token still works for this frame, the new one is asked for
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->requests.at(2).json.value(QStringLiteral("camera_token")).toString(), first);
    QTRY_COMPARE(server->count(QStringLiteral("camera_token")), 2);

    QVERIFY(picture(QStringLiteral("cam1")));
    const QString renewed = server->requests.at(3).json.value(QStringLiteral("camera_token")).toString();
    QVERIFY(renewed != first);
    QCOMPARE(server->requests.at(3).status, 200);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);
    for (const HttpRequest &r: std::as_const(server->requests))
        QVERIFY2(!FakeCalaos::carriesCredentials(r), r.raw.constData());
}

void TstCameraToken::pictureWaitsForCapabilities()
{
    server->holdServerInfo = true;
    connection->setServerInfoTimeout(5000);
    connection->login(TestUser, TestPass, server->url());
    QTRY_VERIFY(server->count(QStringLiteral("get_server_info")) == 1);

    //capabilities not known: cn_pass must not leave yet
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTest::qWait(200);
    QVERIFY(server->requests.isEmpty());
    QCOMPARE(server->count(QStringLiteral("camera_token")), 0);

    server->releaseServerInfo();
    QTRY_COMPARE(downloaded->count(), 1);
    QCOMPARE(server->requests.size(), 1);
    QVERIFY(!server->requests.first().json.value(QStringLiteral("camera_token")).toString().isEmpty());
    QVERIFY(!FakeCalaos::carriesCredentials(server->requests.first()));
}

void TstCameraToken::noAnswerKeepsCnPass()
{
    server->serverInfo = FakeCalaos::Silent;
    login();

    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(downloaded->count(), 1);

    //an old server: no capability, no disconnection, cn_pass as before
    QVERIFY(connection->isWebsocket());
    QCOMPARE(connection->serverCapabilities().state, ServerCapabilities::Unanswered);
    QVERIFY(!connection->serverCapabilities().has(ServerCapabilities::CameraToken));
    QCOMPARE(connection->adminRelaySupport(), AdminTransport::RelaySupport::No);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 0);
    const HttpRequest &r = server->requests.first();
    QCOMPARE(r.status, 200);
    QVERIFY(!r.json.contains(QStringLiteral("camera_token")));
    QCOMPARE(r.json.value(QStringLiteral("cn_user")).toString(), TestUser);
    QCOMPARE(r.json.value(QStringLiteral("cn_pass")).toString(), TestPass);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Connected);
}

void TstCameraToken::errorAnswerKeepsCnPass()
{
    server->serverInfo = FakeCalaos::Error;
    login();
    //an error object is an empty list, not a reason to disconnect
    QTRY_COMPARE(connection->serverCapabilities().state, ServerCapabilities::Answered);
    QVERIFY(connection->serverCapabilities().capabilities.isEmpty());

    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->requests.first().json.value(QStringLiteral("cn_pass")).toString(), TestPass);
    QCOMPARE(connection->adminRelaySupport(), AdminTransport::RelaySupport::No);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Connected);
}

void TstCameraToken::capabilityMissingKeepsCnPass()
{
    server->capabilities = QJsonArray{ QStringLiteral("admin_relay") };
    login();

    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 0);
    QCOMPARE(server->requests.first().json.value(QStringLiteral("cn_pass")).toString(), TestPass);
    QCOMPARE(connection->adminRelaySupport(), AdminTransport::RelaySupport::Yes);
}

void TstCameraToken::refusedTokenNeverFallsBackToCnPass()
{
    //the server announced camera_token, then refuses to give one
    server->cameraTokenReply = FakeCalaos::Error;
    login();
    QTRY_VERIFY(connection->serverCapabilities().has(ServerCapabilities::CameraToken));

    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 1);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);

    //backing off: no new request at once, and the picture fails at once
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 2);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);

    //no answer at all: the wait is bounded
    server->cameraTokenReply = FakeCalaos::Silent;
    advance(1100);
    connection->getCameraPicture(QStringLiteral("cam2"));
    QTRY_COMPARE(failed->count(), 3);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);

    QVERIFY(server->requests.isEmpty());
}

void TstCameraToken::tokenRefusedByHttpIsForgotten()
{
    login();
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);

    //revoked on the server (credentials changed): a 400, and a new token next
    server->revokeTokens();
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 1);
    QCOMPARE(server->requests.last().status, 400);

    advance(1100); //past the backoff
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);
    QCOMPARE(server->requests.last().status, 200);
    for (const HttpRequest &r: std::as_const(server->requests))
        QVERIFY2(!FakeCalaos::carriesCredentials(r), r.raw.constData());
}

void TstCameraToken::refusedTokenBacksOff()
{
    login();
    QVERIFY(picture(QStringLiteral("cam1")));
    server->revokeTokens();
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 1);

    //a 400 is a failure like any other: no token request per frame meanwhile
    for (int i = 0; i < 3; ++i)
    {
        connection->getCameraPicture(QStringLiteral("cam1"));
        QTRY_COMPARE(failed->count(), 2 + i);
    }
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);
    QCOMPARE(server->requests.size(), 2);
}

void TstCameraToken::repeatedRefusalsSpaceOutTokenRequests()
{
    //A token issued is not a token accepted: against a server that refuses
    //everything, the backoff must keep growing (1 s, 2 s...) instead of
    //starting over at each new token.
    server->refuseAll = true;
    login();

    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 1);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);

    //first backoff: 1 s
    advance(1100);
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(server->count(QStringLiteral("camera_token")), 2);
    QTRY_VERIFY(failed->count() >= 2);
    const int refused = server->requests.size();

    //the second backoff lasts 2 s: nothing leaves 1.2 s after the refusal
    advance(1200);
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTest::qWait(100);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);
    QCOMPARE(server->requests.size(), refused);

    //and it does end
    advance(900);
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(server->count(QStringLiteral("camera_token")), 3);
}

void TstCameraToken::acceptedPictureResetsBackoff()
{
    login();
    QVERIFY(picture(QStringLiteral("cam1")));
    server->revokeTokens();
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(failed->count(), 1);

    //first backoff (1 s) over: a new token, accepted this time
    advance(1100);
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(downloaded->count(), 2);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);

    //refused again: back to a 1 s backoff, not 2 s
    server->revokeTokens();
    connection->getCameraPicture(QStringLiteral("cam1"));
    const int before = failed->count();
    QTRY_VERIFY(failed->count() > before);
    advance(1100);
    connection->getCameraPicture(QStringLiteral("cam1"));
    QTRY_COMPARE(server->count(QStringLiteral("camera_token")), 3);
}

void TstCameraToken::lateRefusalKeepsRenewedToken()
{
    server->expiresIn = QStringLiteral("2"); //renewal margin: 1s
    login();
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 1);
    const QString old = server->requests.first().json.value(QStringLiteral("camera_token")).toString();
    server->expiresIn = QStringLiteral("120"); //the renewed one outlives the test

    //sent with the old token and held: its answer comes after the renewal
    server->holdCam = QStringLiteral("late");
    connection->getCameraPicture(QStringLiteral("late"));
    QTRY_COMPARE(server->requests.size(), 2);

    //another camera drives the renewal once the old token is close to its end
    advance(1100);
    auto lastToken = [&]() { return server->requests.last().json.value(QStringLiteral("camera_token")).toString(); };
    QTRY_VERIFY_WITH_TIMEOUT(([&]() {
        return picture(QStringLiteral("cam1")) && lastToken() != old;
    })(), 5000);
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);

    //the server now only knows the renewed token: the held picture is refused
    server->validTokens.remove(old);
    server->releaseHeld();
    QTRY_COMPARE(server->heldStatus, 400);
    QTRY_COMPARE(failed->count(), 1);
    QCOMPARE(failed->last().value(0).toString(), QStringLiteral("late"));

    server->holdCam.clear();
    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);
    QCOMPARE(server->requests.last().status, 200);
}

void TstCameraToken::v1ErrorNeverLogsCredentials()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    //QNetworkReply::errorString() copies the url, u= and p= included
    server->v1Status = 400;
    login();
    QTRY_VERIFY(connection->serverCapabilities().has(ServerCapabilities::CameraToken));

    QStringList lines;
    static QStringList *sink;
    sink = &lines;
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) {
        sink->append(m);
    });
    connection->getCameraPicture(QStringLiteral("cam7"), QStringLiteral("/cam/image?id=7"));
    const bool done = QTest::qWaitFor([&]() { return failed->count() == 1; }, 3000);
    qInstallMessageHandler(previous);
    QVERIFY(done);

    const QString log = lines.join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QLatin1String("Error in")), qPrintable(log));
    //still useful: path, and what happened
    QVERIFY2(log.contains(QLatin1String("/cam/image?id=7")), qPrintable(log));
    QVERIFY2(log.contains(QLatin1String(": 400 ")), qPrintable(log));
    QVERIFY2(log.contains(QLatin1String("p=***")), qPrintable(log));
    for (const QString &secret: { TestPass, QString::fromLatin1(QUrl::toPercentEncoding(TestPass)),
                                  QString::fromLatin1(QUrl::toPercentEncoding(TestUser)), TestUser,
                                  QStringLiteral("x%231"), QStringLiteral("%252F") })
        QVERIFY2(!log.contains(secret), qPrintable(secret + QLatin1String(" in: ") + log));
}

void TstCameraToken::eventPictureNeverLogsCredentials()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    //The picture of a push event used to be an url with cn_user and cn_pass
    //handed to a QML Image, which Qt Quick logs when the load fails. The url
    //QML sees now carries nothing: this is the failing load.
    server->eventPicStatus = 404;
    login();
    QSignalSpy bad(connection, &CalaosConnection::eventPictureFailed);

    QStringList lines;
    static QStringList *sink;
    sink = &lines;
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) {
        sink->append(m);
    });
    connection->getEventPicture(QStringLiteral("pic1"));
    const bool done = QTest::qWaitFor([&]() { return bad.count() == 1; }, 3000);
    qInstallMessageHandler(previous);
    QVERIFY(done);

    const QString log = lines.join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QLatin1String("Error in")) && log.contains(QLatin1String("404")), qPrintable(log));
    for (const QString &secret: { TestPass, QString::fromLatin1(QUrl::toPercentEncoding(TestPass)),
                                  TestUser, QString::fromLatin1(QUrl::toPercentEncoding(TestUser)) })
        QVERIFY2(!log.contains(secret), qPrintable(secret + QLatin1String(" in: ") + log));
}

void TstCameraToken::eventPictureIsFetchedWithBodyCredentials()
{
    login();
    QSignalSpy ok(connection, &CalaosConnection::eventPictureDownloaded);
    connection->getEventPicture(QStringLiteral("pic1"));
    QTRY_COMPARE(ok.count(), 1);
    QCOMPARE(ok.first().value(0).toString(), QStringLiteral("pic1"));
    QVERIFY(!QImage::fromData(ok.first().value(1).toByteArray()).isNull());

    const HttpRequest &r = server->requests.last();
    QCOMPARE(r.json.value(QStringLiteral("action")).toString(), QStringLiteral("event_picture"));
    QCOMPARE(r.json.value(QStringLiteral("pic_uid")).toString(), QStringLiteral("pic1"));
    QCOMPARE(r.json.value(QStringLiteral("cn_pass")).toString(), TestPass);
    QVERIFY2(!r.target.contains("cn_p") && !r.target.contains("cn_u") && !r.target.contains('?'), r.target.constData());
}

void TstCameraToken::eventPictureAnnouncingTooMuchIsRefused()
{
    //The announced length alone refuses it: only 1 KiB ever arrives, so the
    //received-bytes bound cannot be what fires.
    server->eventPicHuge = true;
    login();
    QSignalSpy bad(connection, &CalaosConnection::eventPictureFailed);
    QSignalSpy ok(connection, &CalaosConnection::eventPictureDownloaded);
    connection->setEventPictureTimeout(60000);
    connection->getEventPicture(QStringLiteral("pic1"));
    QTRY_COMPARE_WITH_TIMEOUT(bad.count(), 1, 3000);
    QCOMPARE(ok.count(), 0);
}

void TstCameraToken::eventPictureWithoutAnswerTimesOut()
{
    //The manager only has its 30 s fallback: the request carries its own.
    server->eventPicSilent = true;
    login();
    QSignalSpy bad(connection, &CalaosConnection::eventPictureFailed);
    connection->setEventPictureTimeout(300);
    connection->getEventPicture(QStringLiteral("pic1"));
    QTRY_COMPARE_WITH_TIMEOUT(bad.count(), 1, 3000);
}

void TstCameraToken::eventPictureWithoutLengthIsAbortedAtTheLimit()
{
    //Neither Content-Length nor chunks to count on: only the bytes received
    //can stop it. The transfer timeout is long, so it cannot be that.
    server->big = FakeCalaos::NoLength;
    login();
    QSignalSpy bad(connection, &CalaosConnection::eventPictureFailed);
    QSignalSpy ok(connection, &CalaosConnection::eventPictureDownloaded);
    connection->setEventPictureTimeout(60000);
    connection->getEventPicture(QStringLiteral("pic1"));
    QTRY_COMPARE_WITH_TIMEOUT(bad.count(), 1, 5000);
    QCOMPARE(ok.count(), 0);
    QTRY_COMPARE(server->bigAborted, 1);
}

void TstCameraToken::eventPictureDrippedIsCutByTheTotalTimeout()
{
    server->big = FakeCalaos::Drip;
    login();
    QSignalSpy bad(connection, &CalaosConnection::eventPictureFailed);
    connection->setEventPictureTimeout(1000);
    connection->setEventPictureTotalTimeout(600);
    connection->getEventPicture(QStringLiteral("pic1"));
    //the 200 bytes would take 20 s to arrive
    QTRY_COMPARE_WITH_TIMEOUT(bad.count(), 1, 3000);
}

void TstCameraToken::cameraPictureBoundsAreEnforced()
{
    //read(MaxPictureBytes) in the finish handler is the last guard: each way
    //past the limit must stop the transfer, and the limit itself must pass.
    login();
    for (auto mode: { FakeCalaos::Announced, FakeCalaos::NoLength, FakeCalaos::OverLimit })
    {
        server->big = mode;
        server->bigAborted = 0;
        const int before = failed->count();
        connection->getCameraPicture(QStringLiteral("cam1"), QStringLiteral("/cam/image?id=1"));
        QTRY_COMPARE_WITH_TIMEOUT(failed->count(), before + 1, 5000);
        QCOMPARE(downloaded->count(), 0);
        QTRY_COMPARE(server->bigAborted, 1);
    }

    server->big = FakeCalaos::ExactLimit;
    connection->getCameraPicture(QStringLiteral("cam1"), QStringLiteral("/cam/image?id=1"));
    QTRY_COMPARE_WITH_TIMEOUT(downloaded->count(), 1, 5000);
    QCOMPARE(downloaded->first().value(1).toByteArray().size(), 16 * 1024 * 1024);
}

void TstCameraToken::audioCoverBoundsAreEnforced()
{
    login();
    QSignalSpy cover(connection, &CalaosConnection::audioCoverDownloaded);

    //First: an aborted transfer leaves its connection in a state a later request can trip on.
    server->big = FakeCalaos::ExactLimit;
    connection->getAudioCover(QStringLiteral("p1"));
    QTRY_COMPARE_WITH_TIMEOUT(cover.count(), 1, 5000);
    QCOMPARE(cover.first().value(1).toByteArray().size(), 16 * 1024 * 1024);

    for (auto mode: { FakeCalaos::Announced, FakeCalaos::NoLength, FakeCalaos::OverLimit })
    {
        server->big = mode;
        server->bigAborted = 0;
        connection->getAudioCover(QStringLiteral("p1"));
        if (mode == FakeCalaos::OverLimit)
            QTest::qWait(1500);   //the server does not always notice the client closing after a burst
        else
            QTRY_COMPARE_WITH_TIMEOUT(server->bigAborted, 1, 5000);
        QCOMPARE(cover.count(), 1);
    }
}

void TstCameraToken::v1SuffixWithoutQueryKeepsCredentialsOutOfThePath()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    server->v1Status = 400;
    login();

    QStringList lines;
    static QStringList *sink;
    sink = &lines;
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) {
        sink->append(m);
    });
    connection->getCameraPicture(QStringLiteral("cam7"), QStringLiteral("/cam/image"));
    const bool done = QTest::qWaitFor([&]() { return failed->count() == 1; }, 3000);
    qInstallMessageHandler(previous);
    QVERIFY(done);

    //the server still gets them as a query
    const QByteArray target = server->requests.last().target;
    QVERIFY2(target.startsWith("/cam/image?u="), target.constData());
    QCOMPARE(queryOf(target).value(QStringLiteral("p")), TestPass);

    const QString log = lines.join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QLatin1String("/cam/image")), qPrintable(log));
    QVERIFY2(log.contains(QLatin1String("p=***")), qPrintable(log));
    for (const QString &secret: { TestPass, QString::fromLatin1(QUrl::toPercentEncoding(TestPass)),
                                  QString::fromLatin1(QUrl::toPercentEncoding(TestUser)), TestUser })
        QVERIFY2(!log.contains(secret), qPrintable(secret + QLatin1String(" in: ") + log));
}

void TstCameraToken::eventLogServerErrorNeverLogsPassword()
{
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
    server->eventLogEchoesPassword = true;
    login();
    QSignalSpy done(connection, &CalaosConnection::eventLogPageFinished);

    QStringList lines;
    static QStringList *sink;
    sink = &lines;
    const QtMessageHandler previous = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &m) {
        sink->append(m);
    });
    connection->requestEventLogPage(1, 10);
    const bool got = QTest::qWaitFor([&]() { return done.count() == 1; }, 3000);
    qInstallMessageHandler(previous);
    QVERIFY(got);

    const QString log = lines.join(QLatin1Char('\n'));
    QVERIFY2(log.contains(QLatin1String("failed")), qPrintable(log));
    QVERIFY2(log.contains(QLatin1String("bad request")), qPrintable(log));
    QVERIFY2(!log.contains(TestPass), qPrintable(log));
}

void TstCameraToken::redactionKeepsDiagnosticAndCoversEdgeCases()
{
    //change_cred keys
    const QByteArray j = CalaosConnection::redactJson(QJsonDocument::fromJson(
        R"({"old_pw":"oldsecret","new_pw":"newsecret","kept":"v"})").object());
    QVERIFY2(!j.contains("oldsecret") && !j.contains("newsecret") && j.contains("kept"), j.constData());

    //a short value must not eat the host, the port or the path
    const QUrl url(QStringLiteral("http://127.0.0.1:8000/cam/image?id=1&u=cam&p=1"));
    const QString out = CalaosConnection::redactError(
        QStringLiteral("Error transferring %1 - server replied: Bad Request").arg(url.toString()), url);
    QVERIFY2(out.contains(QLatin1String("127.0.0.1:8000/cam/image?id=1")), qPrintable(out));
    QVERIFY2(out.contains(QLatin1String("p=***")) && !out.contains(QLatin1String("p=1")), qPrintable(out));
    //url absent from the text in that form: only key=value pairs go
    const QString out2 = CalaosConnection::redactError(
        QStringLiteral("failed at 127.0.0.1 with p=1&u=cam"), url);
    QVERIFY2(out2.contains(QLatin1String("127.0.0.1")) && !out2.contains(QLatin1String("p=1")), qPrintable(out2));

    //the url is replaced and the text repeats the pairs elsewhere
    const QString out3 = CalaosConnection::redactError(
        QStringLiteral("Error transferring %1 (retry with p=1&u=cam)").arg(url.toString()), url);
    QVERIFY2(!out3.contains(QLatin1String("p=1")) && out3.contains(QLatin1String("p=***")), qPrintable(out3));

    //credentials pushed into a fragment
    const QUrl frag(QStringLiteral("http://h:1/cam?id=7#x&u=bob&p=s3cret"));
    const QString fo = CalaosConnection::redactError(
        QStringLiteral("Error in %1").arg(frag.toString()), frag);
    QVERIFY2(!fo.contains(QLatin1String("s3cret")) && !fo.contains(QLatin1String("bob")), qPrintable(fo));
}

void TstCameraToken::v1SuffixFragmentDoesNotHideCredentials()
{
    server->v1Status = 400;
    login();
    connection->getCameraPicture(QStringLiteral("cam7"), QStringLiteral("/cam/image?id=7#frag"));
    QTRY_COMPARE(failed->count(), 1);
    //the credentials went into the query, where the server (and the filter) read them
    QVERIFY2(server->requests.last().target.contains("&u="), server->requests.last().target.constData());
    QVERIFY2(!server->requests.last().target.contains('#'), server->requests.last().target.constData());
}

void TstCameraToken::redactErrorAndJson()
{
    const QUrl url(QStringLiteral("http://h:1/cam?id=7&u=bob&p=s3cret%26x&cn_pass=zz"));
    const QString text = QStringLiteral("Error transferring %1 - server replied: Bad Request").arg(url.toString());
    const QString out = CalaosConnection::redactError(text, url);
    QVERIFY2(!out.contains(QLatin1String("s3cret")) && !out.contains(QLatin1String("bob"))
             && !out.contains(QLatin1String("zz")), qPrintable(out));
    QVERIFY(out.contains(QLatin1String("/cam?id=7")));
    QVERIFY(out.contains(QLatin1String("Bad Request")));

    //credentials keep out of nested answers too, other fields stay
    const QJsonDocument doc = QJsonDocument::fromJson(
        R"({"data":{"token":"tok123","camera_token":"ct456","list":[{"cn_pass":"pw789","name":"kept"}]}})");
    const QByteArray j = CalaosConnection::redactJson(doc);
    QVERIFY2(!j.contains("tok123") && !j.contains("ct456") && !j.contains("pw789"), j.constData());
    QVERIFY(j.contains("kept"));
}

void TstCameraToken::v1UrlIsAssembledWithoutArg()
{
    //v1 has no token: the credentials stay in its url, whatever the server announces
    login();
    QTRY_VERIFY(connection->serverCapabilities().has(ServerCapabilities::CameraToken));

    connection->getCameraPicture(QStringLiteral("cam7"), QStringLiteral("/cam/image?id=7"));
    QTRY_COMPARE(downloaded->count(), 1);

    QCOMPARE(server->count(QStringLiteral("camera_token")), 0);
    const HttpRequest &r = server->requests.first();
    QVERIFY(r.target.startsWith("/cam/image?id=7&u="));
    //read the query like the server: each value decoded once
    const QMap<QString, QString> q = queryOf(r.target);
    QCOMPARE(q.value(QStringLiteral("id")), QStringLiteral("7"));
    QCOMPARE(q.value(QStringLiteral("u")), TestUser);
    QCOMPARE(q.value(QStringLiteral("p")), TestPass);
    QCOMPARE(q.size(), 3);
}

QTEST_MAIN(TstCameraToken)
#include "tst_cameratoken.moc"
