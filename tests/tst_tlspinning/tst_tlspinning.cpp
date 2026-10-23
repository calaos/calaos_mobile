/*
 * TLS trust on first use for the Calaos server certificate.
 *
 * Pure logic first: the host:port key, the SubjectPublicKeyInfo walk and its
 * SHA-256 (checked against what openssl computes for the committed fixtures),
 * the decision for every case (trusted by Qt, first contact, pin match, key
 * changed, never acceptable) and the pin store (per host, persistent).
 *
 * Then the handshake path: a real CalaosConnection against TLS servers on
 * 127.0.0.1 whose certificates are generated for the run (no key in the
 * repository). A first contact asks once and does not retry by itself, an
 * accepted certificate connects, a changed key asks again, a refused one
 * stays refused until an explicit login, and nothing loops.
 *
 * Fixtures (tests/data, public certificates only):
 *   tls_a.pem          RSA 2048, key A, serial 1
 *   tls_a_renewed.pem  same key A, serial 2 (a renewal keeping the key)
 *   tls_b.pem          RSA 2048, key B
 *   tls_ec.pem         P-256
 * Expected values: openssl x509 -in <f> -pubkey -noout |
 *                  openssl pkey -pubin -outform der | openssl dgst -sha256
 */

#include <QtTest>
#include <QApplication>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include "CalaosConnection.h"
#include "TlsPinning.h"

using TlsPinning::Decision;

static const QString TestUser = QStringLiteral("user");
static const QString TestPass = QStringLiteral("s3cret-pass");

static const QString SpkiA = QStringLiteral("a1c0f06656149289f7265720f971258a0dbc37ed3968ec88942c4a347a1a6c0c");
static const QString SpkiB = QStringLiteral("9b75e0e202307e5738ceb882ab769e27f29c4f0b204101c5735eddbd1a714041");
static const QString SpkiEc = QStringLiteral("79a46ca09cc1f4e1fca1193eab7589094190628d92dbe46320aad01592900f10");
static const QString CertA = QStringLiteral("a291dbc1548948f46168e716530e6ab582825b44db76539f2fd6bcdaf7b6a29c");
static const QString CertARenewed = QStringLiteral("f002e4d4ba7b9c7a9afe87d00a2938eae142fbbfc050f8e63ba3378713e04e51");

static QSslCertificate fixture(const QString &name)
{
    const QList<QSslCertificate> certs =
        QSslCertificate::fromPath(QStringLiteral(TESTDATA_DIR "/") + name, QSsl::Pem);
    return certs.isEmpty() ? QSslCertificate() : certs.first();
}

static QByteArray derLength(qsizetype len)
{
    if (len < 0x80)
        return QByteArray(1, char(len));
    QByteArray b;
    while (len > 0)
    {
        b.prepend(char(len & 0xff));
        len >>= 8;
    }
    return QByteArray(1, char(0x80 | b.size())) + b;
}

//Header length of the DER element at pos (short or long form).
static qsizetype headerLength(const QByteArray &d, qsizetype pos)
{
    const uchar l = uchar(d.at(pos + 1));
    return 2 + ((l & 0x80) ? (l & 0x7f) : 0);
}

static qsizetype contentLength(const QByteArray &d, qsizetype pos)
{
    const uchar l = uchar(d.at(pos + 1));
    if (!(l & 0x80))
        return l;
    qsizetype len = 0;
    for (int i = 0; i < (l & 0x7f); i++)
        len = (len << 8) | uchar(d.at(pos + 2 + i));
    return len;
}

//The same certificate laid out as X.509 v1 (no [0] version element). The
//signature no longer matches, which the SPKI walk does not care about.
static QByteArray withoutVersion(const QByteArray &der)
{
    const qsizetype tbsPos = headerLength(der, 0);
    const qsizetype tbsHdr = headerLength(der, tbsPos);
    const qsizetype tbsLen = contentLength(der, tbsPos);
    QByteArray tbsBody = der.mid(tbsPos + tbsHdr, tbsLen);
    if (uchar(tbsBody.at(0)) != 0xa0)
        return QByteArray();
    tbsBody.remove(0, headerLength(tbsBody, 0) + contentLength(tbsBody, 0));
    const QByteArray tail = der.mid(tbsPos + tbsHdr + tbsLen);
    const QByteArray tbs = QByteArray(1, char(0x30)) + derLength(tbsBody.size()) + tbsBody;
    const QByteArray body = tbs + tail;
    return QByteArray(1, char(0x30)) + derLength(body.size()) + body;
}

static QString sha256Hex(const QByteArray &b)
{
    return QString::fromLatin1(QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex());
}

static QList<QSslError> selfSigned(const QSslCertificate &c)
{
    return { QSslError(QSslError::SelfSignedCertificate, c),
             QSslError(QSslError::HostNameMismatch, c) };
}

/*
 * calaos_server's websocket API over TLS: answers the login, counts it.
 */
class TlsWsServer: public QWebSocketServer
{
    Q_OBJECT

public:
    explicit TlsWsServer(const QSslConfiguration &tls):
        QWebSocketServer(QStringLiteral("fake calaos_server"), QWebSocketServer::SecureMode)
    {
        setSslConfiguration(tls);
        connect(this, &QWebSocketServer::newConnection, this, [this]()
        {
            while (QWebSocket *ws = nextPendingConnection())
            {
                ws->setParent(this);
                connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString &m)
                {
                    const QJsonObject j = QJsonDocument::fromJson(m.toUtf8()).object();
                    const QString msg = j.value(QStringLiteral("msg")).toString();
                    if (msg == QLatin1String("login"))
                    {
                        logins++;
                        ws->sendTextMessage(QStringLiteral(
                            "{\"msg\":\"login\",\"data\":{\"success\":\"true\"}}"));
                    }
                    else if (msg == QLatin1String("get_home"))
                    {
                        ws->sendTextMessage(QStringLiteral(
                            "{\"msg\":\"get_home\",\"data\":{\"home\":[],\"cameras\":[],\"audio\":[]}}"));
                    }
                });
            }
        });
    }

    int logins = 0;
};

/*
 * calaos_server's api.php over TLS: get_home and the long-poll registration.
 */
class TlsHttpServer: public QTcpServer
{
    Q_OBJECT

public:
    explicit TlsHttpServer(const QSslConfiguration &tls): tlsConfig(tls) {}

    int logins = 0;

protected:
    void incomingConnection(qintptr handle) override
    {
        QSslSocket *sock = new QSslSocket(this);
        sock->setSslConfiguration(tlsConfig);
        sock->setSocketDescriptor(handle);
        sock->startServerEncryption();

        QByteArray *buf = new QByteArray();
        connect(sock, &QSslSocket::readyRead, this, [this, sock, buf]()
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

            const QJsonObject j = QJsonDocument::fromJson(body).object();
            const QString action = j.value(QStringLiteral("action")).toString();
            if (action == QLatin1String("get_home"))
            {
                logins++;
                reply(sock, "{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}");
            }
            else if (action == QLatin1String("poll_listen") &&
                     j.value(QStringLiteral("type")).toString() == QLatin1String("register"))
            {
                reply(sock, "{\"success\":\"true\",\"uuid\":\"fake-poll-uuid\"}");
            }
            else if (action != QLatin1String("poll_listen"))
            {
                reply(sock, "{\"success\":\"true\"}");
            }
            //poll_listen "get": held open, a quiet installation.
        });
        connect(sock, &QSslSocket::disconnected, this, [sock, buf]()
        {
            delete buf;
            sock->deleteLater();
        });
    }

private:
    static void reply(QSslSocket *sock, const QByteArray &body)
    {
        QByteArray out = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                         + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }

    QSslConfiguration tlsConfig;
};

class TstTlsPinning: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();

    //Pure functions
    void hostKey_data();
    void hostKey();
    void spkiMatchesOpenssl_data();
    void spkiMatchesOpenssl();
    void renewalWithSameKeyKeepsThePin();
    void spkiOfV1Layout();
    void spkiOfGarbageIsEmpty();
    void formatFingerprint();
    void decision_data();
    void decision();
    void overridableErrors();

    //TlsPinStore
    void storePerHostAndPersistent();
    void storeForgetAndClear();
    void storeSignalsOnlyOnChange();

    //Handshake path
    void wsFirstUseAsksOnceThenConnects();
    void wsPinnedConnectsWithoutAsking();
    void wsKeyChangeAsksAgain();
    void wsPinIsPerHost();
    void wsRefusalDoesNotLoop();
    void logoutCancelsTheQuestion();
    void httpFirstUseAsksOnceThenConnects();

private:
    static void pump(int ms);
    bool makeServerConfig(const QString &name, QSslConfiguration &config);
    CalaosConnection *newConnection();

    QTemporaryDir dir;
    QSslConfiguration tlsA, tlsB;
    bool tlsReady = false;
    int pinFiles = 0;

    TlsPinStore *store = nullptr;
    CalaosConnection *connection = nullptr;
    TlsWsServer *ws = nullptr;
    TlsHttpServer *http = nullptr;
};

void TstTlsPinning::pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

bool TstTlsPinning::makeServerConfig(const QString &name, QSslConfiguration &config)
{
    const QString key = dir.filePath(name + QStringLiteral("_key.pem"));
    const QString crt = dir.filePath(name + QStringLiteral("_crt.pem"));
    QProcess openssl;
    openssl.start(QStringLiteral("openssl"),
                  { QStringLiteral("req"), QStringLiteral("-x509"),
                    QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
                    QStringLiteral("-nodes"), QStringLiteral("-days"), QStringLiteral("1"),
                    QStringLiteral("-subj"), QStringLiteral("/CN=calaos.local"),
                    QStringLiteral("-keyout"), key, QStringLiteral("-out"), crt });
    if (!openssl.waitForFinished(30000) || openssl.exitStatus() != QProcess::NormalExit ||
        openssl.exitCode() != 0)
        return false;

    QFile kf(key), cf(crt);
    if (!kf.open(QIODevice::ReadOnly) || !cf.open(QIODevice::ReadOnly))
        return false;
    const QSslKey sslKey(kf.readAll(), QSsl::Rsa, QSsl::Pem);
    const QList<QSslCertificate> certs = QSslCertificate::fromData(cf.readAll(), QSsl::Pem);
    if (sslKey.isNull() || certs.isEmpty())
        return false;

    config = QSslConfiguration::defaultConfiguration();
    config.setLocalCertificate(certs.first());
    config.setPrivateKey(sslKey);
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    return true;
}

void TstTlsPinning::initTestCase()
{
    QVERIFY(dir.isValid());
    QVERIFY2(!fixture(QStringLiteral("tls_a.pem")).isNull(), TESTDATA_DIR);

    //Same backend choice as tst_pushnotif: OpenSSL handles PEM keys in
    //server mode everywhere.
    if (QSslSocket::availableBackends().contains(QStringLiteral("openssl")))
        QSslSocket::setActiveBackend(QStringLiteral("openssl"));
    tlsReady = QSslSocket::supportsSsl() &&
               makeServerConfig(QStringLiteral("a"), tlsA) &&
               makeServerConfig(QStringLiteral("b"), tlsB);
}

void TstTlsPinning::cleanup()
{
    if (connection)
    {
        connection->logout();
        pump(50);
        delete connection;
        connection = nullptr;
    }
    delete ws;
    ws = nullptr;
    delete http;
    http = nullptr;
    delete store;
    store = nullptr;
}

CalaosConnection *TstTlsPinning::newConnection()
{
    store = new TlsPinStore(dir.filePath(QStringLiteral("pins%1.ini").arg(++pinFiles)));
    connection = new CalaosConnection();
    connection->setPinStore(store);
    return connection;
}

// ---------------------------------------------------------------- pure functions

void TstTlsPinning::hostKey_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("key");

    QTest::newRow("https default port") << "https://Box.Local/api.php" << "box.local:443";
    QTest::newRow("wss shares 443") << "wss://box.local/api" << "box.local:443";
    QTest::newRow("explicit port") << "wss://192.168.1.10:8443/api" << "192.168.1.10:8443";
    QTest::newRow("http default") << "http://10.0.0.2/api.php" << "10.0.0.2:80";
    QTest::newRow("ws desktop port") << "ws://10.0.0.2:5454/api" << "10.0.0.2:5454";
    QTest::newRow("ddns") << "https://home.example.org/api.php" << "home.example.org:443";
    QTest::newRow("ipv6") << "https://[FE80::1]:4443/api.php" << "[fe80::1]:4443";
}

void TstTlsPinning::hostKey()
{
    QFETCH(QString, url);
    QFETCH(QString, key);
    QCOMPARE(TlsPinning::hostKey(QUrl(url)), key);
}

void TstTlsPinning::spkiMatchesOpenssl_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<QString>("spki");
    QTest::addColumn<QString>("cert");

    QTest::newRow("rsa a") << "tls_a.pem" << SpkiA << CertA;
    QTest::newRow("rsa a renewed") << "tls_a_renewed.pem" << SpkiA << CertARenewed;
    QTest::newRow("rsa b") << "tls_b.pem" << SpkiB << QString();
    QTest::newRow("ec p-256") << "tls_ec.pem" << SpkiEc << QString();
}

void TstTlsPinning::spkiMatchesOpenssl()
{
    QFETCH(QString, file);
    QFETCH(QString, spki);
    QFETCH(QString, cert);

    const QSslCertificate c = fixture(file);
    QVERIFY(!c.isNull());
    QCOMPARE(TlsPinning::spkiSha256(c), spki);
    if (!cert.isEmpty())
        QCOMPARE(TlsPinning::certSha256(c), cert);
}

void TstTlsPinning::renewalWithSameKeyKeepsThePin()
{
    const QSslCertificate a = fixture(QStringLiteral("tls_a.pem"));
    const QSslCertificate renewed = fixture(QStringLiteral("tls_a_renewed.pem"));
    QVERIFY(TlsPinning::certSha256(a) != TlsPinning::certSha256(renewed));
    QCOMPARE(TlsPinning::decide(selfSigned(renewed), TlsPinning::spkiSha256(a),
                                TlsPinning::spkiSha256(renewed)),
             Decision::PinMatch);
}

void TstTlsPinning::spkiOfV1Layout()
{
    const QByteArray der = fixture(QStringLiteral("tls_a.pem")).toDer();
    const QByteArray v1 = withoutVersion(der);
    QVERIFY(!v1.isEmpty());
    QVERIFY(v1.size() < der.size());
    QCOMPARE(sha256Hex(TlsPinning::subjectPublicKeyInfo(v1)), SpkiA);
}

void TstTlsPinning::spkiOfGarbageIsEmpty()
{
    const QByteArray der = fixture(QStringLiteral("tls_a.pem")).toDer();
    QVERIFY(TlsPinning::subjectPublicKeyInfo(QByteArray()).isEmpty());
    QVERIFY(TlsPinning::subjectPublicKeyInfo(QByteArray("not a certificate")).isEmpty());
    //Cut anywhere before the end of the key: never a partial answer.
    for (int n: { 1, 4, 30, 200, 300 })
        QVERIFY2(TlsPinning::subjectPublicKeyInfo(der.left(n)).isEmpty(), qPrintable(QString::number(n)));
    //Length claiming more bytes than there are.
    QByteArray bogus = der;
    bogus[1] = char(0x84);
    QVERIFY(TlsPinning::subjectPublicKeyInfo(bogus).isEmpty());
    QVERIFY(TlsPinning::spkiSha256(QSslCertificate()).isEmpty());
}

void TstTlsPinning::formatFingerprint()
{
    QCOMPARE(TlsPinning::formatFingerprint(QStringLiteral("a1c0f0")), QStringLiteral("A1:C0:F0"));
    QCOMPARE(TlsPinning::formatFingerprint(QString()), QString());
    QCOMPARE(TlsPinning::formatFingerprint(SpkiA).size(), 32 * 3 - 1);
}

void TstTlsPinning::decision_data()
{
    QTest::addColumn<QList<QSslError>>("errors");
    QTest::addColumn<QString>("stored");
    QTest::addColumn<QString>("peer");
    QTest::addColumn<int>("expected");

    const QSslCertificate a = fixture(QStringLiteral("tls_a.pem"));

    QTest::newRow("public CA, no pin") << QList<QSslError>() << QString() << SpkiA
                                       << int(Decision::Trusted);
    QTest::newRow("public CA, other pin") << QList<QSslError>() << SpkiB << SpkiA
                                          << int(Decision::Trusted);
    QTest::newRow("first use") << selfSigned(a) << QString() << SpkiA
                               << int(Decision::AskFirstUse);
    QTest::newRow("match") << selfSigned(a) << SpkiA << SpkiA << int(Decision::PinMatch);
    QTest::newRow("match, other case") << selfSigned(a) << SpkiA.toUpper() << SpkiA
                                       << int(Decision::PinMatch);
    QTest::newRow("key changed") << selfSigned(a) << SpkiB << SpkiA
                                 << int(Decision::AskKeyChanged);
    QTest::newRow("CA cert, LAN IP, pinned")
        << QList<QSslError>{ QSslError(QSslError::HostNameMismatch, a) } << SpkiA << SpkiA
        << int(Decision::PinMatch);
    QTest::newRow("expired self-signed, pinned")
        << QList<QSslError>{ QSslError(QSslError::SelfSignedCertificate, a),
                             QSslError(QSslError::CertificateExpired, a) } << SpkiA << SpkiA
        << int(Decision::PinMatch);
    QTest::newRow("revoked, even pinned")
        << QList<QSslError>{ QSslError(QSslError::HostNameMismatch, a),
                             QSslError(QSslError::CertificateRevoked, a) } << SpkiA << SpkiA
        << int(Decision::Refuse);
    QTest::newRow("bad signature")
        << QList<QSslError>{ QSslError(QSslError::CertificateSignatureFailed, a) } << QString() << SpkiA
        << int(Decision::Refuse);
    QTest::newRow("no certificate")
        << QList<QSslError>{ QSslError(QSslError::NoPeerCertificate) } << SpkiA << QString()
        << int(Decision::Refuse);
    QTest::newRow("unparsable certificate") << selfSigned(a) << SpkiA << QString()
                                            << int(Decision::Refuse);
}

void TstTlsPinning::decision()
{
    QFETCH(QList<QSslError>, errors);
    QFETCH(QString, stored);
    QFETCH(QString, peer);
    QFETCH(int, expected);
    QCOMPARE(int(TlsPinning::decide(errors, stored, peer)), expected);
}

void TstTlsPinning::overridableErrors()
{
    QVERIFY(TlsPinning::isOverridable(QSslError::SelfSignedCertificate));
    QVERIFY(TlsPinning::isOverridable(QSslError::HostNameMismatch));
    QVERIFY(TlsPinning::isOverridable(QSslError::UnableToGetLocalIssuerCertificate));
    QVERIFY(!TlsPinning::isOverridable(QSslError::CertificateRevoked));
    QVERIFY(!TlsPinning::isOverridable(QSslError::CertificateBlacklisted));
    QVERIFY(!TlsPinning::isOverridable(QSslError::NoSslSupport));
}

// ---------------------------------------------------------------- TlsPinStore

void TstTlsPinning::storePerHostAndPersistent()
{
    const QString file = dir.filePath(QStringLiteral("persist.ini"));
    {
        TlsPinStore s(file);
        QVERIFY(s.pins().isEmpty());
        s.setPin(QStringLiteral("192.168.1.10:443"), SpkiA.toUpper());
        s.setPin(QStringLiteral("home.example.org:443"), SpkiB);
        //Stored lowercase whatever the caller gave.
        QCOMPARE(s.pin(QStringLiteral("192.168.1.10:443")), SpkiA);
        //Another port, the overlay IP: other hosts, nothing pinned.
        QCOMPARE(s.pin(QStringLiteral("192.168.1.10:8443")), QString());
        QCOMPARE(s.pin(QStringLiteral("100.64.0.3:443")), QString());
    }

    TlsPinStore again(file);
    QCOMPARE(again.pins().size(), 2);
    QCOMPARE(again.pin(QStringLiteral("192.168.1.10:443")), SpkiA);
    QCOMPARE(again.pin(QStringLiteral("home.example.org:443")), SpkiB);

    //The key of A pinned for the LAN IP says nothing about the DDNS name.
    const QSslCertificate a = fixture(QStringLiteral("tls_a.pem"));
    QCOMPARE(TlsPinning::decide(selfSigned(a), again.pin(QStringLiteral("192.168.1.10:443")), SpkiA),
             Decision::PinMatch);
    QCOMPARE(TlsPinning::decide(selfSigned(a), again.pin(QStringLiteral("home.example.org:443")), SpkiA),
             Decision::AskKeyChanged);
    QCOMPARE(TlsPinning::decide(selfSigned(a), again.pin(QStringLiteral("100.64.0.3:443")), SpkiA),
             Decision::AskFirstUse);
}

void TstTlsPinning::storeForgetAndClear()
{
    const QString file = dir.filePath(QStringLiteral("forget.ini"));
    TlsPinStore s(file);
    s.setPin(QStringLiteral("a:443"), SpkiA);
    s.setPin(QStringLiteral("b:443"), SpkiB);

    QVERIFY(s.forget(QStringLiteral("a:443")));
    QVERIFY(!s.forget(QStringLiteral("a:443")));
    QCOMPARE(s.pin(QStringLiteral("a:443")), QString());
    QCOMPARE(TlsPinStore(file).pin(QStringLiteral("b:443")), SpkiB);

    s.clear();
    QVERIFY(s.pins().isEmpty());
    QVERIFY(TlsPinStore(file).pins().isEmpty());
}

void TstTlsPinning::storeSignalsOnlyOnChange()
{
    TlsPinStore s(dir.filePath(QStringLiteral("signals.ini")));
    QSignalSpy changed(&s, &TlsPinStore::pinsChanged);

    s.setPin(QStringLiteral("a:443"), SpkiA);
    s.setPin(QStringLiteral("a:443"), SpkiA);
    QCOMPARE(changed.count(), 1);
    s.setPin(QStringLiteral("a:443"), SpkiB);
    QCOMPARE(changed.count(), 2);
    s.forget(QStringLiteral("nope:443"));
    QCOMPARE(changed.count(), 2);
    s.clear();
    s.clear();
    QCOMPARE(changed.count(), 3);
}

// ---------------------------------------------------------------- handshake path

void TstTlsPinning::wsFirstUseAsksOnceThenConnects()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    ws = new TlsWsServer(tlsA);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    const QString key = TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws->serverPort());
    const QString spki = TlsPinning::spkiSha256(tlsA.localCertificate());

    newConnection();
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);
    //Bare host: wss:// first, https:// as the fallback.
    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(ws->serverPort()));

    QTRY_COMPARE(asked.count(), 1);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    const QVariantMap info = asked.at(0).at(0).toMap();
    QCOMPARE(info.value(QStringLiteral("host")).toString(), key);
    QCOMPARE(info.value(QStringLiteral("fingerprint")).toString(), TlsPinning::formatFingerprint(spki));
    QCOMPARE(info.value(QStringLiteral("certFingerprint")).toString(),
             TlsPinning::formatFingerprint(TlsPinning::certSha256(tlsA.localCertificate())));
    QCOMPARE(info.value(QStringLiteral("keyChanged")).toBool(), false);
    QVERIFY(connection->hasPendingTlsTrust());

    //Waiting for the user: no login went through, nothing retries (not even
    //the HTTP fallback or server discovery), nothing is pinned yet.
    QVERIFY(!connection->acceptsImmediateReconnect());
    pump(1500);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(ws->logins, 0);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(store->pin(key), QString());

    connection->answerTlsTrust(true);
    QCOMPARE(store->pin(key), spki);
    QVERIFY(!connection->hasPendingTlsTrust());
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(connection->isWebsocket());
    QCOMPARE(ws->logins, 1);
    QCOMPARE(asked.count(), 1);
}

void TstTlsPinning::wsPinnedConnectsWithoutAsking()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    ws = new TlsWsServer(tlsA);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    newConnection();
    store->setPin(TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws->serverPort()),
                  TlsPinning::spkiSha256(tlsA.localCertificate()));
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);

    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(ws->serverPort()));
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(asked.count(), 0);
    QCOMPARE(ws->logins, 1);
}

void TstTlsPinning::wsKeyChangeAsksAgain()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    //The box got a new key (certificate regenerated, renewal without
    //--reuse-key): the old pin must not let it through silently.
    ws = new TlsWsServer(tlsB);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    const QString key = TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws->serverPort());
    const QString oldSpki = TlsPinning::spkiSha256(tlsA.localCertificate());
    const QString newSpki = TlsPinning::spkiSha256(tlsB.localCertificate());
    newConnection();
    store->setPin(key, oldSpki);
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);

    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(ws->serverPort()));
    QTRY_COMPARE(asked.count(), 1);
    const QVariantMap info = asked.at(0).at(0).toMap();
    QCOMPARE(info.value(QStringLiteral("keyChanged")).toBool(), true);
    QCOMPARE(info.value(QStringLiteral("previousFingerprint")).toString(),
             TlsPinning::formatFingerprint(oldSpki));
    QCOMPARE(info.value(QStringLiteral("fingerprint")).toString(),
             TlsPinning::formatFingerprint(newSpki));
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(ws->logins, 0);
    QCOMPARE(store->pin(key), oldSpki);

    connection->answerTlsTrust(true);
    QCOMPARE(store->pin(key), newSpki);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QCOMPARE(asked.count(), 1);
}

void TstTlsPinning::wsPinIsPerHost()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    //The same key pinned for another name or another port of the box.
    ws = new TlsWsServer(tlsA);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    const QString spki = TlsPinning::spkiSha256(tlsA.localCertificate());
    newConnection();
    store->setPin(TlsPinning::hostKey(QStringLiteral("localhost"), ws->serverPort()), spki);
    store->setPin(TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws->serverPort() + 1), spki);
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);

    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(ws->serverPort()));
    QTRY_COMPARE(asked.count(), 1);
    QCOMPARE(asked.at(0).at(0).toMap().value(QStringLiteral("keyChanged")).toBool(), false);
    QCOMPARE(ws->logins, 0);
}

void TstTlsPinning::wsRefusalDoesNotLoop()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    ws = new TlsWsServer(tlsA);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    const QString host = QStringLiteral("127.0.0.1:%1").arg(ws->serverPort());
    const QString key = TlsPinning::hostKey(QStringLiteral("127.0.0.1"), ws->serverPort());
    newConnection();
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);
    QSignalSpy rejected(connection, &CalaosConnection::tlsTrustRejected);

    connection->login(TestUser, TestPass, host);
    QTRY_COMPARE(asked.count(), 1);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);

    connection->answerTlsTrust(false);
    QCOMPARE(rejected.count(), 1);
    QCOMPARE(rejected.at(0).at(0).toString(), key);
    QCOMPARE(store->pin(key), QString());
    pump(1500);
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(asked.count(), 1);

    //Logins the app starts by itself (launch, iOS become-active) go through
    //login() too: the refused certificate fails them without a new question.
    connection->login(TestUser, TestPass, host);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    pump(500);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(ws->logins, 0);

    //An explicit login from the login screen asks again.
    connection->forgetTlsRefusals();
    connection->login(TestUser, TestPass, host);
    QTRY_COMPARE(asked.count(), 2);
    QCOMPARE(ws->logins, 0);
    //A second answer for the same question is ignored.
    connection->answerTlsTrust(false);
    connection->answerTlsTrust(true);
    QCOMPARE(store->pin(key), QString());
    QCOMPARE(rejected.count(), 2);
}

void TstTlsPinning::logoutCancelsTheQuestion()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    ws = new TlsWsServer(tlsA);
    QVERIFY(ws->listen(QHostAddress::LocalHost, 0));
    newConnection();
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);
    QSignalSpy canceled(connection, &CalaosConnection::tlsTrustCanceled);

    connection->login(TestUser, TestPass, QStringLiteral("127.0.0.1:%1").arg(ws->serverPort()));
    QTRY_COMPARE(asked.count(), 1);
    connection->logout();
    QCOMPARE(canceled.count(), 1);
    QVERIFY(!connection->hasPendingTlsTrust());

    //A late "accept" from a dialog still on screen must not log in.
    connection->answerTlsTrust(true);
    pump(500);
    QVERIFY(store->pins().isEmpty());
    QCOMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(ws->logins, 0);
}

void TstTlsPinning::httpFirstUseAsksOnceThenConnects()
{
    if (!tlsReady)
        QSKIP("No TLS backend or no openssl CLI to make a test certificate");

    http = new TlsHttpServer(tlsA);
    QVERIFY(http->listen(QHostAddress::LocalHost, 0));
    const QString key = TlsPinning::hostKey(QStringLiteral("127.0.0.1"), http->serverPort());
    newConnection();
    QSignalSpy asked(connection, &CalaosConnection::tlsTrustRequested);

    connection->login(TestUser, TestPass,
                      QStringLiteral("https://127.0.0.1:%1/api.php").arg(http->serverPort()));
    QTRY_COMPARE(asked.count(), 1);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Disconnected);
    QCOMPARE(asked.at(0).at(0).toMap().value(QStringLiteral("host")).toString(), key);
    pump(1500);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(http->logins, 0);

    connection->answerTlsTrust(true);
    QTRY_COMPARE(connection->connectionState(), ReconnectPolicy::Connected);
    QVERIFY(connection->isHttp());
    QCOMPARE(http->logins, 1);
    QCOMPARE(asked.count(), 1);
}

QTEST_MAIN(TstTlsPinning)
#include "tst_tlspinning.moc"
