/*
 * NetworkRequest and WeatherModel against an in-process HTTP server: the log
 * must carry neither the query credentials (APPID, p, token...), nor the
 * secrets of a JSON body, whether the request succeeds or fails.
 * The values below are synthetic.
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include "NetworkRequest.h"
#include "LogRedact.h"
#include "WeatherInfo.h"

namespace {

const QString Appid = QStringLiteral("dummyappid0123456789abcdef");
const QString Secret = QStringLiteral("hunter2-dummy");

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

class TstLogRedact: public QObject
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

    void weatherErrorNeverLogsAppid();
    void freeTextHidesAddresses();
    void networkRequestErrorNeverLogsSecrets();
    void networkRequestSuccessNeverLogsSecrets();

private:
    QString base() const { return QStringLiteral("http://127.0.0.1:%1/").arg(server->serverPort()); }
    QString log() const { return lines.join(QLatin1Char('\n')); }

    HttpServer *server = nullptr;
    QStringList lines;
    QtMessageHandler previous = nullptr;
};

void TstLogRedact::weatherErrorNeverLogsAppid()
{
    server->status = 500;
    WeatherModel model;
    model.setApiBase(base());
    model.refreshWeather();
    QTRY_VERIFY_WITH_TIMEOUT(log().contains(QLatin1String("forecast")), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(log().count(QLatin1String("Error in weather request")) >= 2, 3000);

    const QString l = log();
    //The key is a constant of the program: a hex run after APPID= is it.
    QVERIFY2(!QRegularExpression(QStringLiteral("APPID=[0-9a-fA-F]{8,}")).match(l).hasMatch(), qPrintable(l));
    QVERIFY2(l.contains(QLatin1String("APPID=***")), qPrintable(l));
}

void TstLogRedact::freeTextHidesAddresses()
{
    const QString out = LogRedact::redactFreeText(QStringLiteral("no route to 192.0.2.10 from 00:00:5e:00:53:01 (p=1)"));
    QVERIFY2(!out.contains(QLatin1String("192.0.2.10")), qPrintable(out));
    QVERIFY2(!out.contains(QLatin1String("00:00:5e")), qPrintable(out));
    QVERIFY(out.contains(QLatin1String("no route")));
}

void TstLogRedact::networkRequestErrorNeverLogsSecrets()
{
    server->status = 403;
    server->body = QByteArrayLiteral(R"({"error":"denied","cn_pass":"hunter2-dummy","token":"hunter2-dummy"})");

    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api?APPID=%1&p=%2&mode=json").arg(Appid, Secret));
    req.setNetManager(&nam);
    req.setRequestType(NetworkRequest::HttpPost);
    req.setPostData(R"({"cn_pass":"hunter2-dummy"})");
    req.setCustomHeader(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + Secret);
    QSignalSpy done(&req, &NetworkRequest::finishedJson);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 3000);

    const QString l = log();
    QVERIFY2(l.contains(QLatin1String("Error in")), qPrintable(l));
    QVERIFY2(l.contains(QLatin1String("status 403")), qPrintable(l)); //still useful
    //The error body is never dumped, redacted or not.
    QVERIFY2(!l.contains(QLatin1String("denied")), qPrintable(l));
    for (const QString &secret: { Appid, Secret })
        QVERIFY2(!l.contains(secret), qPrintable(secret + QLatin1String(" in: ") + l));
}

void TstLogRedact::networkRequestSuccessNeverLogsSecrets()
{
    server->status = 200;
    server->body = QByteArrayLiteral(R"({"success":"true","cn_pass":"hunter2-dummy","camera_token":"hunter2-dummy"})");

    QNetworkAccessManager nam;
    NetworkRequest req(base() + QStringLiteral("api?APPID=%1").arg(Appid));
    req.setNetManager(&nam);
    QSignalSpy done(&req, &NetworkRequest::finishedJson);
    QVERIFY(req.start());
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 3000);

    const QString l = log();
    QVERIFY2(l.contains(QLatin1String("New request to")), qPrintable(l));
    //Not even redacted: the body is never dumped.
    QVERIFY2(!l.contains(QLatin1String("camera_token")), qPrintable(l));
    for (const QString &secret: { Appid, Secret })
        QVERIFY2(!l.contains(secret), qPrintable(secret + QLatin1String(" in: ") + l));
}
QTEST_MAIN(TstLogRedact)
#include "tst_logredact.moc"
