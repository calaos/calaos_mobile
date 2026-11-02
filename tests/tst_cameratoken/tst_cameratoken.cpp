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
        QByteArray body = "JPEG";
        if (action == QLatin1String("camera") || action == QLatin1String("get_camera_pic"))
        {
            if (r.json.contains(QStringLiteral("camera_token")))
            {
                if (!validTokens.contains(r.json.value(QStringLiteral("camera_token")).toString()))
                    r.status = 400;
            }
            else if (action == QLatin1String("camera") &&
                     (r.json.value(QStringLiteral("cn_user")).toString() != TestUser ||
                      r.json.value(QStringLiteral("cn_pass")).toString() != TestPass))
            {
                r.status = 401;
            }
        }
        else
        {
            r.status = 400;
        }
        requests << r;

        if (r.status != 200)
            body = "refused";
        QByteArray out = "HTTP/1.1 " + QByteArray::number(r.status) + " X\r\nContent-Type: image/jpeg";
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
    void v1UrlIsAssembledWithoutArg();
    void notifPictureUrlKeepsCnPass();

private:
    void login();
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
    const int before = downloaded->count() + failed->count();
    connection->getCameraPicture(camid);
    return QTest::qWaitFor([&]() { return downloaded->count() + failed->count() > before; }, 3000)
           && downloaded->count() + failed->count() == before + 1
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

    QTest::qWait(1150);
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
    QTest::qWait(1100);
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

    QVERIFY(picture(QStringLiteral("cam1")));
    QCOMPARE(server->count(QStringLiteral("camera_token")), 2);
    QCOMPARE(server->requests.last().status, 200);
    for (const HttpRequest &r: std::as_const(server->requests))
        QVERIFY2(!FakeCalaos::carriesCredentials(r), r.raw.constData());
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

void TstCameraToken::notifPictureUrlKeepsCnPass()
{
    //notification images are out of the token's reach: cn_pass, encoded as UTF-8
    login();
    const QUrl url(connection->getNotifPictureUrl(QStringLiteral("pic&1")));
    QVERIFY(url.isValid());
    const QMap<QString, QString> q = queryOf(url.toEncoded());
    QCOMPARE(q.value(QStringLiteral("cn_user")), TestUser);
    QCOMPARE(q.value(QStringLiteral("cn_pass")), TestPass);
    QCOMPARE(q.value(QStringLiteral("pic_uid")), QStringLiteral("pic&1"));
    QCOMPARE(q.size(), 4);
    QVERIFY(!url.toEncoded().contains("camera_token"));
}

QTEST_MAIN(TstCameraToken)
#include "tst_cameratoken.moc"
