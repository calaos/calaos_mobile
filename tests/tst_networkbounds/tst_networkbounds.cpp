/*
 * NetworkRequest and WeatherModel against an in-process HTTP server that
 * answers more than the bound allows: announced by Content-Length, streamed
 * without any length, or sent whole. Each must end in an error, and the
 * server must see the client let go of the connection.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "NetworkLimits.h"
#include "NetworkRequest.h"
#include "WeatherInfo.h"

namespace {

enum Mode { Announced, NoLength, OverLimit };

class HttpServer: public QTcpServer
{
public:
    Mode mode = Announced;
    //Bound of the code under test.
    qint64 limit = 0;
    int released = 0;

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]()
        {
            sock->readAll();
            if (sock->property("answered").toBool())
                return;
            sock->setProperty("answered", true);
            auto counted = std::make_shared<bool>(false);
            auto count = [this, counted]() { if (!*counted) { *counted = true; ++released; } };
            connect(sock, &QTcpSocket::disconnected, this, count);
            connect(sock, &QTcpSocket::errorOccurred, this, count);

            //One answer per connection: a client reusing it would wait for a second one.
            const QByteArray head = "HTTP/1.1 200 X\r\nContent-Type: application/json\r\nConnection: close\r\n";
            switch (mode)
            {
            case Announced:
                //only 1 KiB ever arrives: the announced length alone must refuse it
                sock->write(head + "Content-Length: " + QByteArray::number(limit + 1024) + "\r\n\r\n" + QByteArray(1024, 'x'));
                break;
            case NoLength:
            {
                sock->write(head + "\r\n");
                QTimer *t = new QTimer(sock);
                t->setInterval(1);
                connect(t, &QTimer::timeout, sock, [sock]() {
                    if (sock->bytesToWrite() < 1024 * 1024)
                        sock->write(QByteArray(256 * 1024, 'x'));
                });
                t->start();
                break;
            }
            case OverLimit:
                sock->write(head + "Content-Length: " + QByteArray::number(limit + 1) + "\r\n\r\n" + QByteArray(limit + 1, 'x'));
                break;
            }
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }
};

QStringList *sink = nullptr;

void capture(QtMsgType, const QMessageLogContext &, const QString &m)
{
    sink->append(m);
}

} // namespace

class TstNetworkBounds: public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("*.debug=true"));
        server = new HttpServer();
        QVERIFY(server->listen(QHostAddress::LocalHost, 0));
        lines.clear();
        sink = &lines;
        previous = qInstallMessageHandler(capture);
    }

    void cleanup()
    {
        qInstallMessageHandler(previous);
        delete server;
    }

    void jsonRequestIsBounded_data() { modes(); }
    void jsonRequestIsBounded();
    void rawRequestIsBounded_data() { modes(); }
    void rawRequestIsBounded();
    void weatherIsBounded_data() { modes(); }
    void weatherIsBounded();

private:
    static void modes()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("announced") << int(Announced);
        QTest::newRow("no length") << int(NoLength);
        QTest::newRow("over the limit") << int(OverLimit);
    }
    QString base() const { return QStringLiteral("http://127.0.0.1:%1/").arg(server->serverPort()); }
    QString log() const { return lines.join(QLatin1Char('\n')); }
    void runRequest(NetworkRequest::ResulType type, qint64 limit);

    HttpServer *server = nullptr;
    QStringList lines;
    QtMessageHandler previous = nullptr;
};

void TstNetworkBounds::runRequest(NetworkRequest::ResulType type, qint64 limit)
{
    QFETCH(int, mode);
    server->mode = Mode(mode);
    server->limit = limit;

    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(type);
    QSignalSpy json(&req, &NetworkRequest::finishedJson);
    QSignalSpy data(&req, &NetworkRequest::finishedData);
    QVERIFY(req.start());
    QTRY_VERIFY_WITH_TIMEOUT(json.count() + data.count() == 1, 8000);

    if (type == NetworkRequest::TypeJson)
        QCOMPARE(json.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    else
        QCOMPARE(data.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QVERIFY2(log().contains(QLatin1String("Error in")), qPrintable(log()));
    if (mode != OverLimit)
        QTRY_COMPARE_WITH_TIMEOUT(server->released, 1, 5000);
}

void TstNetworkBounds::jsonRequestIsBounded()
{
    runRequest(NetworkRequest::TypeJson, NetworkLimits::MaxJsonBytes);
}

void TstNetworkBounds::rawRequestIsBounded()
{
    runRequest(NetworkRequest::TypeRawData, NetworkLimits::MaxPictureBytes);
}

void TstNetworkBounds::weatherIsBounded()
{
    QFETCH(int, mode);
    server->mode = Mode(mode);
    server->limit = NetworkLimits::MaxWeatherBytes;

    WeatherModel model;
    model.setApiBase(base());
    model.refreshWeather();
    //the forecast request follows the weather one, both are refused
    QTRY_VERIFY2_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, qPrintable(log()), 8000);
}

QTEST_MAIN(TstNetworkBounds)
#include "tst_networkbounds.moc"
