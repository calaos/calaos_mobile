/*
 * With the default logging rules (no QT_LOGGING_RULES) a successful request
 * writes nothing: the kiosk polls the daemon every few seconds and its journal
 * must stay readable. Values are synthetic.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "NetworkRequest.h"

namespace {

class HttpServer: public QTcpServer
{
public:
    int status = 200;
    QByteArray body;
    QByteArray lastRequest;

protected:
    void incomingConnection(qintptr handle) override
    {
        QTcpSocket *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]()
        {
            lastRequest += sock->readAll();
            if (!lastRequest.contains("\r\n\r\n"))
                return;
            QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json";
            out += "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            sock->write(out);
            sock->disconnectFromHost();
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

class TstNetQuiet: public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
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

    void successfulRequestIsSilentByDefault();
    void failedRequestIsOneWarningLine();

private:
    QString base() const { return QStringLiteral("http://127.0.0.1:%1/").arg(server->serverPort()); }

    HttpServer *server = nullptr;
    QStringList lines;
    QtMessageHandler previous = nullptr;
};

void TstNetQuiet::successfulRequestIsSilentByDefault()
{
    server->body = QByteArrayLiteral(R"({"hostname":"box","ip":"192.0.2.10","mac":"00:00:5e:00:53:01"})");

    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api/v2/system/info"));
    req.setNetManager(&nam);
    QSignalSpy done(&req, &NetworkRequest::finishedJson);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 3000);
    QCOMPARE(done.first().first().toInt(), int(NetworkRequest::RequestSuccess));

    QVERIFY2(lines.isEmpty(), qPrintable(lines.join(QLatin1Char('\n'))));
}

void TstNetQuiet::failedRequestIsOneWarningLine()
{
    server->status = 500;
    server->body = QByteArrayLiteral(R"({"detail":"192.0.2.10 secret-dummy"})");

    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api/v2/system/info"));
    req.setNetManager(&nam);
    QSignalSpy done(&req, &NetworkRequest::finishedJson);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 3000);

    QCOMPARE(lines.size(), 1);
    QVERIFY2(lines.first().contains(QLatin1String("status 500")), qPrintable(lines.first()));
    QVERIFY2(!lines.first().contains(QLatin1String("secret-dummy")), qPrintable(lines.first()));
}

int main(int argc, char **argv)
{
    //The test runners export QT_LOGGING_RULES="*.debug=true": this suite is
    //about the rules a kiosk runs with.
    qunsetenv("QT_LOGGING_RULES");
    QApplication app(argc, argv);
    TstNetQuiet tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "tst_netquiet.moc"
