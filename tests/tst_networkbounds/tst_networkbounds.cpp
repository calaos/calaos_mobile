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

enum Mode { Announced, NoLength, OverLimit, Chunked, Deflate, DeflateModerate, Burst, Small, Exact, Redirect, Mute, Drip };

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
    QString location;

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
            case Redirect:
                sock->write("HTTP/1.1 302 Found\r\nLocation: " + location.toUtf8() + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                sock->disconnectFromHost();
                break;
            case Mute:
                break;
            case Drip:
            {
                //One byte every 20 ms, never ending: the transfer timeout never fires.
                sock->write(head + "Content-Length: 1000000\r\n\r\n");
                QTimer *t = new QTimer(sock);
                t->setInterval(20);
                connect(t, &QTimer::timeout, sock, [sock]() { sock->write("x"); });
                t->start();
                break;
            }
            case Exact:
                sock->write(head + "Content-Length: " + QByteArray::number(limit) + "\r\n\r\n" + QByteArray(limit, 'x'));
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

//A file that refuses every write and cannot be emptied, as on a full disk.
class FullDiskFile: public QFile
{
public:
    using QFile::QFile;
    bool resize(qint64) override { return false; }

protected:
    qint64 writeData(const char *, qint64) override { return -1; }
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
    void missingFileFailsOnceWithItsMessage();
    void retryAfterFailureDoesNotAppend();
    void fileThatCannotBeWrittenFailsTheRequest();
    void partialFileIsRemovedWhenItCannotBeEmptied();
    void fileOpenedByTheCallerIsNotRemoved();
    void slotMayRestartTheRequestAfterAnError_data();
    void slotMayRestartTheRequestAfterAnError();
    void slotMayDeleteTheRequestWhileItReads();
    void staleReplyIsIgnoredAfterARestart_data();
    void staleReplyIsIgnoredAfterARestart();
    void slotMayDeleteLaterTheRequestWithItsOwnManager();
    void boundStartsOverAfterTheFileWasTruncated();
    void fileDestroyedMidTransferFailsTheRequest();
    void fileDestroyedBeforeTheRequestIsSafe();
    void webSocketFrameIsBounded();
    void webSocketFrameHeaderIsRefusedBeforeItsBody();
    void weatherRepliesAreFreed();
    void weatherDefaultsToHttps();
    void weatherRefusesRedirectToAnotherHost();
    void weatherSilentServerIsCutByTimeout();
    void weatherDrippingServerIsCutByDeadline();
    void weatherRefreshReplacesRequestInFlight();
    void weatherRefreshAbandonsForecastInFlight();

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
        //Ratio ~30: passes Qt's check, only the reader's sum cuts it.
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
    //The sum is strict whatever the batches. Only what Qt or the headers refuse
    //before any readyRead hands over nothing.
    const bool refusedUpFront = (mode == Announced || mode == OverLimit || mode == Deflate);
    QVERIFY2(handedOver <= (refusedUpFront ? 0 : limit), qPrintable(QString::number(handedOver)));

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
    //The failed file is truncated before finished: what was written is counted
    //from what the reader was handed, after the write.
    qint64 handedOver = 0;
    connect(&req, &NetworkRequest::dataReadyRead, this, [&](const QByteArray &d) { handedOver += d.size(); });
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);

    //Once the bound has aborted the reply, nothing more is written or handed over.
    const int chunksAtFinish = chunks.count();
    const qint64 sizeAtFinish = file.size();
    QTest::qWait(150);
    QCOMPARE(chunks.count(), chunksAtFinish);
    QCOMPARE(file.size(), sizeAtFinish);

    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QVERIFY2(handedOver <= bound, qPrintable(QString::number(handedOver)));
    //The partial file is not left open for a retry to append to.
    QVERIFY(!file.isOpen());
    QCOMPARE(file.size(), qint64(0));
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
    //Our own cut says why, not a bare "Operation canceled".
    QVERIFY2(req.getLastError().contains(QLatin1String("larger")), qPrintable(req.getLastError()));
    QVERIFY2(handedOver <= bound, qPrintable(QString::number(handedOver)));
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
    //Reported by the return value only, and no request leaves.
    QTest::qWait(300);
    QCOMPARE(done.count(), 0);
    QCOMPARE(server->released, 0);
}

void TstNetworkBounds::missingFileFailsOnceWithItsMessage()
{
    server->mode = Small;
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::TypeFile);
    QSignalSpy done(&req, &NetworkRequest::finished);
    QVERIFY(!req.start());
    QVERIFY2(req.getLastError().contains(QLatin1String("dlFile")), qPrintable(req.getLastError()));
    QTest::qWait(300);
    QCOMPARE(done.count(), 0);
    QCOMPARE(server->released, 0);
}

void TstNetworkBounds::retryAfterFailureDoesNotAppend()
{
    const qint64 bound = 64 * 1024;
    server->mode = Burst;
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
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));

    server->mode = Small;
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 8000);
    QCOMPARE(done.last().value(0).toInt(), int(NetworkRequest::RequestSuccess));
    QCOMPARE(file.size(), qint64(5));
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

void TstNetworkBounds::partialFileIsRemovedWhenItCannotBeEmptied()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FullDiskFile file(dir.filePath(QStringLiteral("download")));
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
    QVERIFY(!file.isOpen());
    QVERIFY(!QFileInfo::exists(file.fileName()));
}

void TstNetworkBounds::fileOpenedByTheCallerIsNotRemoved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FullDiskFile file(dir.filePath(QStringLiteral("download")));
    QVERIFY(file.open(QFile::WriteOnly));
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
    QVERIFY(QFileInfo::exists(file.fileName()));
}

void TstNetworkBounds::slotMayRestartTheRequestAfterAnError_data()
{
    QTest::addColumn<int>("type");
    QTest::newRow("file") << int(NetworkRequest::TypeFile);
    QTest::newRow("json") << int(NetworkRequest::TypeJson);
    QTest::newRow("raw data") << int(NetworkRequest::TypeRawData);
}

void TstNetworkBounds::slotMayRestartTheRequestAfterAnError()
{
    QFETCH(int, type);
    server->mode = Burst;
    server->limit = 64 * 1024;
    QTemporaryFile file;
    QVERIFY(file.open());
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::ResulType(type));
    if (type == NetworkRequest::TypeFile)
        req.setFile(&file);
    req.setMaxBytes(64 * 1024);
    int calls = 0;
    bool restarted = false;
    auto done = [&]()
    {
        if (calls++ == 0)
        {
            server->mode = Small;
            restarted = req.start();
        }
    };
    connect(&req, &NetworkRequest::finished, this, done);
    connect(&req, &NetworkRequest::finishedJson, this, done);
    connect(&req, &NetworkRequest::finishedData, this, done);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(calls, 2, 8000);
    QVERIFY(restarted);
    if (type == NetworkRequest::TypeFile)
        QCOMPARE(file.size(), qint64(5));
}

void TstNetworkBounds::slotMayDeleteTheRequestWhileItReads()
{
    //ASan: the request is read again after the signal if the slot deletes it.
    server->mode = NoLength;
    server->limit = NetworkLimits::MaxJsonBytes;
    QNetworkAccessManager nam;
    auto *req = new NetworkRequest(base() + QStringLiteral("api"));
    req->setNetManager(&nam);
    req->setResultType(NetworkRequest::TypeJson);
    int chunks = 0;
    connect(req, &NetworkRequest::dataReadyRead, this, [&]()
    {
        if (chunks++ == 0)
            delete req;
    });
    QVERIFY(req->start());
    QTRY_VERIFY_WITH_TIMEOUT(chunks > 0, 8000);
    QTest::qWait(150);
    QCOMPARE(chunks, 1);
}

void TstNetworkBounds::staleReplyIsIgnoredAfterARestart_data()
{
    QTest::addColumn<bool>("toFile");
    QTest::newRow("data") << false;
    QTest::newRow("file") << true;
}

void TstNetworkBounds::staleReplyIsIgnoredAfterARestart()
{
    QFETCH(bool, toFile);
    //A batch over the bound aborts the reply from limitDownload, which runs
    //finished and so the slot below before the reader's own readyRead slot.
    server->mode = NoLength;
    server->limit = NetworkLimits::MaxPictureBytes;
    QTemporaryFile file;
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    if (toFile)
    {
        QVERIFY(file.open());
        req.setResultType(NetworkRequest::TypeFile);
        req.setFile(&file);
    }
    else
        req.setResultType(NetworkRequest::TypeRawData);
    req.setMaxBytes(64 * 1024);
    int emptyChunks = 0;
    int finishedCalls = 0;
    bool restarted = false;
    connect(&req, &NetworkRequest::dataReadyRead, this, [&](const QByteArray &d)
    {
        if (d.isEmpty())
            ++emptyChunks;
    });
    auto restartOnce = [&]()
    {
        if (finishedCalls++ == 0)
            restarted = req.start();
    };
    connect(&req, &NetworkRequest::finishedData, this, restartOnce);
    connect(&req, &NetworkRequest::finished, this, restartOnce);
    QVERIFY(req.start());
    QTRY_VERIFY_WITH_TIMEOUT(finishedCalls >= 2, 8000);
    QVERIFY(restarted);
    //An empty chunk is the old readyRead slot reading the new reply.
    QCOMPARE(emptyChunks, 0);
}

void TstNetworkBounds::slotMayDeleteLaterTheRequestWithItsOwnManager()
{
    server->mode = Small;
    auto *req = new NetworkRequest(base() + QStringLiteral("api"));
    req->setResultType(NetworkRequest::TypeRawData);
    QPointer<NetworkRequest> guard(req);
    QSignalSpy done(req, &NetworkRequest::finishedData);
    connect(req, &NetworkRequest::finishedData, this, [req]() { req->deleteLater(); });
    QVERIFY(req->start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestSuccess));
    QTRY_VERIFY_WITH_TIMEOUT(guard.isNull(), 3000);
}

void TstNetworkBounds::boundStartsOverAfterTheFileWasTruncated()
{
    //After a failed attempt the file is empty again: a retry may receive up to
    //the bound, counted from zero.
    const qint64 bound = 64 * 1024;
    server->mode = Burst;
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
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));

    server->mode = Exact;
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 8000);
    QCOMPARE(done.last().value(0).toInt(), int(NetworkRequest::RequestSuccess));
    QCOMPARE(file.size(), bound);

    //and the bound still holds on that second attempt
    server->mode = Burst;
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 3, 8000);
    QCOMPARE(done.last().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QCOMPARE(file.size(), qint64(0));
    //Cut by the file sum, not by limitDownload.
    QVERIFY2(req.getLastError().contains(QLatin1String("larger")), qPrintable(req.getLastError()));
}

void TstNetworkBounds::fileDestroyedMidTransferFailsTheRequest()
{
    server->mode = NoLength;
    server->limit = NetworkLimits::MaxFileBytes;
    auto *file = new QTemporaryFile();
    QVERIFY(file->open());
    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api"));
    req.setNetManager(&nam);
    req.setResultType(NetworkRequest::TypeFile);
    req.setFile(file);
    QSignalSpy done(&req, &NetworkRequest::finished);
    bool destroyed = false;
    connect(&req, &NetworkRequest::dataReadyRead, this, [&]()
    {
        if (!destroyed)
            delete file;
        destroyed = true;
    });
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
    QCOMPARE(done.first().value(0).toInt(), int(NetworkRequest::RequestHttpError));
    QVERIFY(!req.getFile());
    QVERIFY2(req.getLastError().contains(QLatin1String("no longer exists")), qPrintable(req.getLastError()));
}

void TstNetworkBounds::fileDestroyedBeforeTheRequestIsSafe()
{
    server->mode = NoLength;
    server->limit = NetworkLimits::MaxFileBytes;
    auto *file = new QTemporaryFile();
    QVERIFY(file->open());
    QNetworkAccessManager nam;
    auto *req = new NetworkRequest(base() + QStringLiteral("api"));
    req->setNetManager(&nam);
    req->setResultType(NetworkRequest::TypeFile);
    req->setFile(file);
    QSignalSpy chunks(req, &NetworkRequest::dataReadyRead);
    QVERIFY(req->start());
    QTRY_VERIFY_WITH_TIMEOUT(chunks.count() > 0, 8000);
    //the destructor aborts the transfer, which reaches the finished handler
    delete file;
    delete req;
    QTest::qWait(100);
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

void TstNetworkBounds::weatherRefusesRedirectToAnotherHost()
{
    //Another port is another origin; this one must never be contacted.
    QTcpServer other;
    QVERIFY(other.listen(QHostAddress::LocalHost, 0));
    QSignalSpy contacted(&other, &QTcpServer::pendingConnectionAvailable);
    server->mode = Redirect;
    server->location = QStringLiteral("http://127.0.0.1:%1/stolen").arg(other.serverPort());

    WeatherModel model;
    model.setApiBase(base());
    model.refreshWeather();
    QTRY_VERIFY2_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, qPrintable(log()), 8000);
    QTest::qWait(100);
    QCOMPARE(contacted.count(), 0);
}

void TstNetworkBounds::weatherSilentServerIsCutByTimeout()
{
    server->mode = Mute;

    WeatherModel model;
    model.setApiBase(base());
    model.setTransferTimeout(300);
    model.refreshWeather();
    QTRY_VERIFY2_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, qPrintable(log()), 8000);
}

void TstNetworkBounds::weatherDrippingServerIsCutByDeadline()
{
    server->mode = Drip;

    WeatherModel model;
    model.setApiBase(base());
    //The default transfer timeout (30 s) must not be what ends the test.
    model.setTotalTimeout(500);
    model.refreshWeather();
    QTRY_VERIFY2_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, qPrintable(log()), 8000);
}

void TstNetworkBounds::weatherRefreshReplacesRequestInFlight()
{
    server->mode = Mute;

    WeatherModel model;
    model.setApiBase(base());
    model.refreshWeather();
    model.refreshWeather();
    model.refreshWeather();

    auto running = [&model]() {
        int n = 0;
        for (QNetworkReply *r : model.findChildren<QNetworkReply *>())
            if (r->isRunning())
                ++n;
        return n;
    };
    QTRY_COMPARE_WITH_TIMEOUT(running(), 1, 3000);
    //the abandoned requests must not chain into a forecast request
    QTest::qWait(200);
    QCOMPARE(running(), 1);
    QVERIFY2(!log().contains(QLatin1String("Error in weather request")), qPrintable(log()));
}

void TstNetworkBounds::weatherRefreshAbandonsForecastInFlight()
{
    server->mode = Mute;

    WeatherModel model;
    model.setApiBase(base());
    model.setTransferTimeout(1000);
    model.refreshWeather();
    //weather has timed out, the forecast request is now in flight
    QTRY_COMPARE_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")), 1, 10000);

    model.refreshWeather();
    int running = 0;
    for (QNetworkReply *r : model.findChildren<QNetworkReply *>())
        if (r->isRunning())
            ++running;
    QCOMPARE(running, 1);
    //the abandoned forecast must not report; the new weather request is still within its 1 s
    QTest::qWait(150);
    QCOMPARE(log().count(QLatin1String("Error in weather request")), 1);
}

QTEST_MAIN(TstNetworkBounds)
#include "tst_networkbounds.moc"
