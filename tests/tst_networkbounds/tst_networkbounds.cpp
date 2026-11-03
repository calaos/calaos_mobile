/*
 * NetworkRequest and WeatherModel against an in-process HTTP server that
 * answers more than the bound allows: announced by Content-Length, streamed
 * without any length, or sent whole. Each must end in an error, and the
 * server must see the client let go of the connection.
 */

#include <QtTest>
#include <QApplication>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include "NetworkLimits.h"
#include "NetworkRequest.h"
#include "WeatherInfo.h"

namespace {

enum Mode { Announced, NoLength, OverLimit, Chunked, Deflate, DeflateModerate, Burst, Small };

//Deflate stream (no size prefix) of about `inflated` bytes, ratio ~30: under
//the ratio 40 above which Qt refuses an archive bomb before any readyRead.
QByteArray moderateDeflate(qint64 inflated)
{
    QRandomGenerator rng(1);
    QByteArray raw;
    raw.reserve(inflated);
    while (raw.size() < inflated)
    {
        raw.append(char(rng.bounded(256)));
        raw.append(QByteArray(60, 'x'));
    }
    return qCompress(raw).mid(4);
}

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
                //Whole answers end the connection themselves instead of relying on Connection: close.
                sock->disconnectFromHost();
                break;
            case Chunked:
            {
                sock->write(head + "Transfer-Encoding: chunked\r\n\r\n");
                QTimer *t = new QTimer(sock);
                t->setInterval(1);
                connect(t, &QTimer::timeout, sock, [sock]() {
                    if (sock->bytesToWrite() < 1024 * 1024)
                        sock->write("40000\r\n" + QByteArray(256 * 1024, 'x') + "\r\n");
                });
                t->start();
                break;
            }
            case Deflate:
            {
                //A few hundred bytes on the wire, over the bound once inflated:
                //qCompress is a 4 byte size prefix followed by a zlib stream.
                const QByteArray body = qCompress(QByteArray(limit + 1024, 'x')).mid(4);
                sock->write(head + "Content-Encoding: deflate\r\nContent-Length: " + QByteArray::number(body.size())
                            + "\r\n\r\n" + body);
                sock->disconnectFromHost();
                break;
            }
            case Burst:
            {
                //Chunks of limit/4 each, the whole under two limits: no batch
                //is over the bound, only their sum is. Ends by itself.
                sock->write(head + "Transfer-Encoding: chunked\r\n\r\n");
                auto left = std::make_shared<int>(8);
                const QByteArray piece = QByteArray::number(limit / 4, 16) + "\r\n" + QByteArray(limit / 4, 'x') + "\r\n";
                QTimer *t = new QTimer(sock);
                t->setInterval(2);
                connect(t, &QTimer::timeout, sock, [sock, t, left, piece]() {
                    if (*left > 0)
                    {
                        sock->write(piece);
                        --*left;
                    }
                    else
                    {
                        sock->write("0\r\n\r\n");
                        sock->disconnectFromHost();
                        t->stop();
                    }
                });
                t->start();
                break;
            }
            case Small:
                sock->write(head + "Content-Length: 5\r\n\r\nhello");
                sock->disconnectFromHost();
                break;
            case DeflateModerate:
            {
                const QByteArray body = moderateDeflate(limit + 1024);
                sock->write(head + "Content-Encoding: deflate\r\nContent-Length: " + QByteArray::number(body.size())
                            + "\r\n\r\n" + body);
                sock->disconnectFromHost();
                break;
            }
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
    void fileRequestIsBounded_data() { modes(); }
    void fileRequestIsBounded();
    void moderateDeflateStaysUnderQtArchiveCheck();
    void cumulativeBoundCountsWhatTheReaderTook_data();
    void cumulativeBoundCountsWhatTheReaderTook();
    void fileThatCannotBeOpenedFailsTheRequest();
    void fileThatCannotBeWrittenFailsTheRequest();
    void webSocketFrameIsBounded();
    void webSocketFrameHeaderIsRefusedBeforeItsBody();
    void weatherRepliesAreFreed();
    void weatherDefaultsToHttps();

private:
    static void modes()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("announced") << int(Announced);
        QTest::newRow("no length") << int(NoLength);
        QTest::newRow("over the limit") << int(OverLimit);
        QTest::newRow("chunked, no length") << int(Chunked);
        //Ratio ~1000: Qt refuses it itself, before the readyRead hook.
        QTest::newRow("small on the wire, over once inflated") << int(Deflate);
        //Ratio ~30: passes Qt's check, only the reader's sum cuts it, whatever
        //the size of the readyRead batches.
        QTest::newRow("moderate ratio, over once inflated") << int(DeflateModerate);
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
    int chunks = 0;
    int emptyChunks = 0;
    qint64 handedOver = 0;
    connect(&req, &NetworkRequest::dataReadyRead, this, [&](const QByteArray &d)
    {
        ++chunks;
        handedOver += d.size();
        if (d.isEmpty())
            ++emptyChunks;
    });
    QVERIFY(req.start());
    QTRY_VERIFY_WITH_TIMEOUT(json.count() + data.count() == 1, 8000);

    //Once the bound has aborted the reply, nothing more is handed over.
    const int atFinish = chunks;
    QTest::qWait(150);
    QCOMPARE(chunks, atFinish);
    //An empty chunk is what a readyRead handler reads from an already aborted reply.
    QCOMPARE(emptyChunks, 0);
    //Streamed bodies are read as they come, so up to a few chunks pass before the
    //cut; a refused answer (announced, whole, or inflated) hands over nothing.
    const bool streamed = (mode == NoLength || mode == Chunked);
    QVERIFY2(handedOver <= (streamed ? limit + 4 * 1024 * 1024 : 0), qPrintable(QString::number(handedOver)));

    if (type == NetworkRequest::TypeJson)
        QCOMPARE(json.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    else
        QCOMPARE(data.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QVERIFY2(log().contains(QLatin1String("Error in")), qPrintable(log()));
    if (mode != OverLimit && mode != Deflate && mode != DeflateModerate)
        QTRY_COMPARE_WITH_TIMEOUT(server->released, 1, 5000);
}

void TstNetworkBounds::fileRequestIsBounded()
{
    QFETCH(int, mode);
    const qint64 bound = 64 * 1024;
    server->mode = Mode(mode);
    server->limit = bound;

    QTemporaryFile file;
    QVERIFY(file.open());
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::TypeFile);
    req.setFile(&file);
    req.setMaxBytes(bound);
    QSignalSpy done(&req, &NetworkRequest::finished);
    QSignalSpy chunks(&req, &NetworkRequest::dataReadyRead);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);

    //Once the bound has aborted the reply, nothing more is written or handed over.
    const int chunksAtFinish = chunks.count();
    const qint64 sizeAtFinish = file.size();
    QTest::qWait(150);
    QCOMPARE(chunks.count(), chunksAtFinish);
    QCOMPARE(file.size(), sizeAtFinish);

    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    //The overrun is cut within a few chunks of the bound, not after the whole body.
    QVERIFY2(file.size() <= bound + 4 * 1024 * 1024, qPrintable(QString::number(file.size())));
}

void TstNetworkBounds::moderateDeflateStaysUnderQtArchiveCheck()
{
    //The DeflateModerate rows prove the readyRead hook only while Qt lets them through.
    const qint64 inflated = 4 * 1024 * 1024;
    const double ratio = double(inflated) / moderateDeflate(inflated).size();
    QVERIFY2(ratio > 15 && ratio < 39, qPrintable(QString::number(ratio)));
}

void TstNetworkBounds::cumulativeBoundCountsWhatTheReaderTook_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("json") << int(NetworkRequest::TypeJson);
    QTest::newRow("raw") << int(NetworkRequest::TypeRawData);
    QTest::newRow("file") << int(NetworkRequest::TypeFile);
}

void TstNetworkBounds::cumulativeBoundCountsWhatTheReaderTook()
{
    QFETCH(int, type);
    const qint64 bound = 64 * 1024;
    server->mode = Burst;
    server->limit = bound;

    QTemporaryFile file;
    QVERIFY(file.open());
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::ResulType(type));
    req.setFile(&file);
    req.setMaxBytes(bound);
    QSignalSpy json(&req, &NetworkRequest::finishedJson);
    QSignalSpy data(&req, &NetworkRequest::finishedData);
    QSignalSpy done(&req, &NetworkRequest::finished);
    qint64 handedOver = 0;
    connect(&req, &NetworkRequest::dataReadyRead, this, [&](const QByteArray &d) { handedOver += d.size(); });
    QVERIFY(req.start());
    QTRY_VERIFY_WITH_TIMEOUT(json.count() + data.count() + done.count() == 1, 8000);

    const int status = json.count() ? json.first().value(0).toInt()
                     : data.count() ? data.first().value(0).toInt() : done.first().value(0).toInt();
    QCOMPARE(status, int(NetworkRequest::RequestHttpError));
    QVERIFY2(handedOver <= bound, qPrintable(QString::number(handedOver)));
    if (type == NetworkRequest::TypeFile)
        QVERIFY2(file.size() <= bound, qPrintable(QString::number(file.size())));
}

void TstNetworkBounds::fileThatCannotBeOpenedFailsTheRequest()
{
    server->mode = Small;
    QFile file(QStringLiteral("/nonexistent-dir/calaos-download"));
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::TypeFile);
    req.setFile(&file);
    QSignalSpy done(&req, &NetworkRequest::finished);
    QVERIFY(!req.start());
    QVERIFY2(req.getLastError().contains(QLatin1String("open")), qPrintable(req.getLastError()));
    //Any late signal must not report a success.
    QTest::qWait(300);
    for (const auto &args: done)
        QCOMPARE(args.value(0).toInt(), int(NetworkRequest::RequestHttpError));
}

void TstNetworkBounds::fileThatCannotBeWrittenFailsTheRequest()
{
    //The device /dev/full opens fine and refuses every write (ENOSPC).
    QFile file(QStringLiteral("/dev/full"));
    if (!file.exists())
        QSKIP("no /dev/full");
    server->mode = Small;
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::TypeFile);
    req.setFile(&file);
    QSignalSpy done(&req, &NetworkRequest::finished);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QVERIFY2(req.getLastError().contains(QLatin1String("write")), qPrintable(req.getLastError()));
}

void TstNetworkBounds::webSocketFrameIsBounded()
{
    QWebSocketServer wss(QStringLiteral("t"), QWebSocketServer::NonSecureMode);
    QVERIFY(wss.listen(QHostAddress::LocalHost, 0));
    QWebSocket *peer = nullptr;
    connect(&wss, &QWebSocketServer::newConnection, this, [&]() {
        peer = wss.nextPendingConnection();
        peer->sendTextMessage(QString(int(NetworkLimits::MaxJsonBytes + 1024), QLatin1Char('x')));
    });

    QWebSocket ws;
    NetworkLimits::limitWebSocket(&ws);
    QSignalSpy closed(&ws, &QWebSocket::disconnected);
    QSignalSpy got(&ws, &QWebSocket::textMessageReceived);
    ws.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(wss.serverPort())));
    QTRY_COMPARE_WITH_TIMEOUT(closed.count(), 1, 10000);
    QCOMPARE(got.count(), 0);
    QCOMPARE(ws.closeCode(), QWebSocketProtocol::CloseCodeTooMuchData);
}

void TstNetworkBounds::webSocketFrameHeaderIsRefusedBeforeItsBody()
{
    //A frame header announcing more than the bound, followed by a few bytes only: the
    //message bound cannot act before the frame is complete, the frame bound must.
    QTcpServer raw;
    QVERIFY(raw.listen(QHostAddress::LocalHost, 0));
    QByteArray request;
    connect(&raw, &QTcpServer::newConnection, this, [&]()
    {
        QTcpSocket *peer = raw.nextPendingConnection();
        connect(peer, &QTcpSocket::readyRead, this, [&request, peer]()
        {
            request += peer->readAll();
            if (!request.contains("\r\n\r\n") || peer->property("upgraded").toBool())
                return;
            peer->setProperty("upgraded", true);
            QByteArray key;
            for (const QByteArray &line: request.split('\n'))
                if (line.toLower().startsWith("sec-websocket-key:"))
                    key = line.mid(line.indexOf(':') + 1).trimmed();
            const QByteArray accept = QCryptographicHash::hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
                                                               QCryptographicHash::Sha1).toBase64();
            QByteArray frame;
            frame.append(char(0x81));
            frame.append(char(127));
            const quint64 announced = NetworkLimits::MaxJsonBytes + 1;
            for (int shift = 56; shift >= 0; shift -= 8)
                frame.append(char((announced >> shift) & 0xff));
            frame.append(QByteArray(16, 'x'));
            peer->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n" + frame);
        });
    });

    QWebSocket ws;
    NetworkLimits::limitWebSocket(&ws);
    QSignalSpy got(&ws, &QWebSocket::textMessageReceived);
    ws.open(QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(raw.serverPort())));
    QTRY_COMPARE_WITH_TIMEOUT(ws.closeCode(), QWebSocketProtocol::CloseCodeTooMuchData, 5000);
    QCOMPARE(got.count(), 0);
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

void TstNetworkBounds::weatherRepliesAreFreed()
{
    server->mode = OverLimit;
    server->limit = NetworkLimits::MaxWeatherBytes;

    WeatherModel model;
    model.setApiBase(base());
    model.refreshWeather();
    QTRY_VERIFY2_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, qPrintable(log()), 8000);
    //deleteLater() is processed by the event loop
    QTRY_COMPARE_WITH_TIMEOUT(model.findChildren<QNetworkReply *>().size(), 0, 3000);
}

void TstNetworkBounds::weatherDefaultsToHttps()
{
    WeatherModel model;
    QVERIFY2(model.getApiBase().startsWith(QLatin1String("https://")), qPrintable(model.getApiBase()));
}

QTEST_MAIN(TstNetworkBounds)
#include "tst_networkbounds.moc"
