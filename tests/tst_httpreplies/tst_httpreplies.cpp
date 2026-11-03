/*
 * Lifetime of the HTTP replies tracked by CalaosConnection (FIX-3).
 *
 * Bug: requestFinished(), requestCamFinished() and requestAudioCoverFinished()
 * deleted a failed reply (deleteLater()) but returned before removing it from
 * reqReplies. The next resetTransport() - logout, reconnection, iOS going to
 * the background - then called abort() on the freed object. A camera picture
 * that fails is common (camera offline), so this was a crash waiting for the
 * next reconnection.
 *
 * Each case makes one request fail, lets the reply really be deleted, then
 * tears the transport down. The test binary is built with AddressSanitizer
 * (see the .pro), which reports the use-after-free deterministically.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>

#include "CalaosConnection.h"

/*
 * A calaos_server whose login and long-poll work but which answers every
 * other request (commands, camera pictures, audio covers) with a 500.
 */
class FailingApiServer: public QTcpServer
{
    Q_OBJECT

public:
    using QTcpServer::QTcpServer;

    //Requests answered with an error, per action.
    QStringList failedActions;

    QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1/api.php").arg(serverPort());
    }

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
    void handleRequest(QTcpSocket *sock, const QJsonObject &j)
    {
        const QString action = j.value(QStringLiteral("action")).toString();

        if (action == QLatin1String("get_home"))
        {
            reply(sock, 200, "{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}");
            return;
        }

        if (action == QLatin1String("poll_listen"))
        {
            if (j.value(QStringLiteral("type")).toString() == QLatin1String("register"))
                reply(sock, 200, "{\"success\":\"true\",\"uuid\":\"fake-poll-uuid\"}");
            //"get": held open, like a long-poll with no event.
            return;
        }

        failedActions << action;
        reply(sock, 500, "<html>Internal Server Error</html>");
    }

    static void reply(QTcpSocket *sock, int code, const QByteArray &body)
    {
        if (sock->state() != QAbstractSocket::ConnectedState)
            return;
        QByteArray out = "HTTP/1.1 " + QByteArray::number(code)
                         + (code == 200 ? " OK" : " Internal Server Error")
                         + "\r\nContent-Type: application/json";
        out += "\r\nContent-Length: " + QByteArray::number(body.size());
        out += "\r\nConnection: close\r\n\r\n";
        out += body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }
};

class TstHttpReplies: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void failedCameraPictureThenLogout();
    void failedCameraPictureThenReconnect();
    void failedAudioCoverThenLogout();
    void failedCommandThenLogout();

private:
    void login();
    //Lets the failed reply run its handler and really be deleted.
    static void settle();

    FailingApiServer *server = nullptr;
    CalaosConnection *connection = nullptr;
};

void TstHttpReplies::init()
{
    server = new FailingApiServer();
    QVERIFY(server->listen(QHostAddress::LocalHost, 0));
    connection = new CalaosConnection();
}

void TstHttpReplies::cleanup()
{
    delete connection;
    connection = nullptr;
    delete server;
    server = nullptr;
}

void TstHttpReplies::login()
{
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), server->baseUrl());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
}

void TstHttpReplies::settle()
{
    QTest::qWait(200);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(50);
}

void TstHttpReplies::failedCameraPictureThenLogout()
{
    login();

    QSignalSpy failed(connection, &CalaosConnection::cameraPictureFailed);
    connection->getCameraPicture(QStringLiteral("camera_1"));
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(server->failedActions, QStringList { QStringLiteral("camera") });
    settle();

    //resetTransport() walks reqReplies: the failed reply must not be there.
    connection->logout();
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    settle();
}

void TstHttpReplies::failedCameraPictureThenReconnect()
{
    login();

    QSignalSpy failed(connection, &CalaosConnection::cameraPictureFailed);
    for (int i = 1; i <= 3; i++)
        connection->getCameraPicture(QStringLiteral("camera_%1").arg(i));
    QTRY_COMPARE(failed.count(), 3);
    settle();

    //A new attempt tears the previous transport down first.
    connection->logout();
    login();
    connection->getCameraPicture(QStringLiteral("camera_1"));
    QTRY_COMPARE(failed.count(), 4);
    settle();
    connection->logout();
    settle();
}

void TstHttpReplies::failedAudioCoverThenLogout()
{
    login();

    connection->getAudioCover(QStringLiteral("player_1"));
    QTRY_COMPARE(server->failedActions, QStringList { QStringLiteral("audio") });
    //No signal on a failed cover: give the reply time to come back.
    settle();

    connection->logout();
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    settle();
}

void TstHttpReplies::failedCommandThenLogout()
{
    login();

    //Over HTTP a command goes through sendHttp() / requestFinished().
    connection->sendCommand(QStringLiteral("output_1"), QStringLiteral("true"),
                            QStringLiteral("output"), QStringLiteral("set_state"));
    QTRY_COMPARE(server->failedActions.size(), 1);
    settle();

    connection->logout();
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    settle();
}

QTEST_MAIN(TstHttpReplies)
#include "tst_httpreplies.moc"
