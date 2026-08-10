/*
 * Tests unitaires de src/CameraModel.cpp — cadence de polling des cameras (T15).
 *
 * Bug reproduit : avant le correctif, chaque passage de cameraVisible a true
 * demarrait une chaine de polling independante. startCamera() faisait un
 * QTimer::singleShot(0) inconditionnel, et cameraPictureDownloaded() /
 * cameraPictureFailed() re-armaient chacun un QTimer::singleShot(200) sans
 * qu'aucun objet ne puisse arreter la chaine precedente. Basculer la
 * visibilite (navigation vers la vue camera puis retour, en boucle)
 * multipliait donc le nombre de chaines, donc le debit de requetes vers
 * calaos_server, jusqu'a saturer le reseau et le serveur.
 *
 * Correctif : un unique QTimer single-shot par CameraItem est le seul objet
 * capable de re-armer une requete. Redemarrer ce timer remplace le timeout en
 * attente au lieu d'en ajouter un second, ce qui plafonne structurellement le
 * nombre de chaines a une par camera. Un drapeau pollInFlight couvre le cas ou
 * la visibilite bascule pendant un aller-retour reseau, et un chien de garde de
 * 5 s relance la chaine si une reponse est perdue.
 *
 * Invariant verifie ici : quelle que soit la sequence de bascules de
 * visibilite, il ne reste jamais qu'une seule chaine par camera. En regime
 * etabli les requetes d'une camera sont donc serialisees (jamais plus proches
 * que l'intervalle de polling), et le debit apres une rafale de bascules
 * revient exactement a ce qu'il etait avant.
 *
 * Ce que le correctif ne borne PAS, et que ces tests documentent plutot qu'ils
 * ne l'interdisent : startCamera() envoie une requete immediate quand aucune
 * chaine n'est armee, et stopCamera() desarme le timer sans memoriser la date
 * de la derniere requete. Afficher puis masquer la camera plus vite que
 * l'intervalle de polling coute donc une requete par affichage, sans plancher
 * temporel. Le debit pendant la rafale est borne par la cadence de navigation
 * (une requete par affichage), pas par l'intervalle de polling - c'est ce que
 * verifie burstBudget ci-dessous. Avant T15 c'etait bien pire : chaque
 * affichage demarrait une chaine supplementaire qui survivait a la rafale, et
 * le debit restait durablement multiplie.
 *
 * Perimetre du lien (voir tst_camerapolling.pro) : le test instancie le vrai
 * CameraModel, donc un vrai CalaosConnection (les requetes partent par
 * QNetworkAccessManager) et un vrai QQmlApplicationEngine (CameraModel
 * enregistre son image provider dessus). Les requetes sont servies par un
 * serveur HTTP minimal en process, ce qui rend la mesure hermetique : aucun
 * calaos_server, aucun acces reseau hors boucle locale.
 */

#include <QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "CalaosConnection.h"
#include "CameraModel.h"
#include "ModelImageProvider.h"

//Intervalle nominal entre deux requetes d'une meme camera (CameraModel.cpp).
static const int PollIntervalMs = 200;
//Chien de garde de CameraItem (CameraModel.cpp).
static const int WatchdogMs = 5000;

/*
 * Serveur HTTP minimal qui joue le role de calaos_server pour ce test :
 * repond a get_home, sert une image pour get_picture, et horodate chaque
 * requete d'image par camera.
 */
class FakeApiServer: public QTcpServer
{
    Q_OBJECT

public:
    explicit FakeApiServer(QObject *parent = nullptr): QTcpServer(parent)
    {
        clock.start();
    }

    //Delai applique avant de repondre a une requete d'image (aller-retour).
    int replyDelayMs = 20;
    //Cameras dont les reponses sont avalees (simulation d'une reponse perdue).
    QSet<QString> swallowed;

    //Horodatage (ms depuis le demarrage du serveur) de chaque requete recue.
    QMultiHash<QString, qint64> requests;

    QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1/api.php").arg(serverPort());
    }

    void resetRequests() { requests.clear(); }

    QList<qint64> stampsFor(const QString &camId) const
    {
        QList<qint64> l = requests.values(camId);
        std::sort(l.begin(), l.end());
        return l;
    }

    int countIn(const QString &camId, qint64 from, qint64 to) const
    {
        int n = 0;
        for (qint64 t: stampsFor(camId))
            if (t >= from && t < to)
                n++;
        return n;
    }

    qint64 now() const { return clock.elapsed(); }

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
            const QByteArray hdr = buf->left(hdrEnd);
            int contentLength = 0;
            for (const QByteArray &line: hdr.split('\n'))
            {
                if (line.toLower().startsWith("content-length:"))
                    contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            }
            const QByteArray body = buf->mid(hdrEnd + 4);
            if (body.size() < contentLength)
                return;
            buf->clear();
            handleRequest(sock, body);
        });
        connect(sock, &QTcpSocket::disconnected, this, [sock, buf]()
        {
            delete buf;
            sock->deleteLater();
        });
    }

private:
    void handleRequest(QTcpSocket *sock, const QByteArray &body)
    {
        const QJsonObject j = QJsonDocument::fromJson(body).object();
        const QString type = j.value(QStringLiteral("type")).toString();

        if (type == QLatin1String("get_picture"))
        {
            const QString camId = j.value(QStringLiteral("camera_id")).toString();
            requests.insert(camId, clock.elapsed());
            if (swallowed.contains(camId))
                return; //never answer: exercises the watchdog
            QTimer::singleShot(replyDelayMs, sock, [this, sock]()
            {
                if (sock->state() == QAbstractSocket::ConnectedState)
                    reply(sock, "image/jpeg", jpeg());
            });
            return;
        }

        //Anything else (get_home, poll_listen, ...) gets a harmless answer.
        reply(sock, "application/json",
              QByteArray("{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}"));
    }

    static void reply(QTcpSocket *sock, const char *mime, const QByteArray &body)
    {
        QByteArray out = "HTTP/1.1 200 OK\r\nContent-Type: ";
        out += mime;
        out += "\r\nContent-Length: " + QByteArray::number(body.size());
        out += "\r\nConnection: close\r\n\r\n";
        out += body;
        sock->write(out);
        sock->flush();
        sock->disconnectFromHost();
    }

    static QByteArray jpeg()
    {
        //A real, decodable 2x2 JPEG so that QImage::fromData() succeeds and the
        //image really lands in the cache read by the provider.
        static QByteArray data;
        if (data.isEmpty())
        {
            QImage img(2, 2, QImage::Format_RGB32);
            img.fill(Qt::darkCyan);
            QBuffer b(&data);
            b.open(QIODevice::WriteOnly);
            img.save(&b, "JPEG");
        }
        return data;
    }

    QElapsedTimer clock;
};

class TstCameraPolling: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    //Une seule chaine par camera en regime etabli.
    void steadyStateKeepsOnePollingChainPerCamera();
    //L'invariant tient malgre des bascules rapides de visibilite.
    void rapidVisibilityTogglesDoNotMultiplyTheRate();
    //Une reponse perdue ne gele pas la camera : le chien de garde relance.
    void watchdogRestartsTheChainAfterALostReply();
    //Le cache d'images supporte des lectures concurrentes hors thread GUI.
    void imageCacheSurvivesConcurrentProviderReads();

private:
    void loadCameras(int n);
    void pump(int ms);

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    CameraModel *model = nullptr;
    FakeApiServer *server = nullptr;
};

void TstCameraPolling::init()
{
    server = new FakeApiServer();
    QVERIFY(server->listen(QHostAddress::LocalHost, 0));

    engine = new QQmlApplicationEngine();
    connection = new CalaosConnection();
    model = new CameraModel(engine, connection);

    //Sets httphost, which is what getCameraPicture() posts to. The fake server
    //answers get_home so the connection settles instead of retrying.
    connection->login(QStringLiteral("user"), QStringLiteral("pass"), server->baseUrl());
    pump(200);
}

void TstCameraPolling::cleanup()
{
    model->set_cameraVisible(false);
    pump(100);
    delete model;
    model = nullptr;
    delete connection;
    connection = nullptr;
    delete engine;
    engine = nullptr;
    delete server;
    server = nullptr;
}

void TstCameraPolling::loadCameras(int n)
{
    QVariantList cams;
    for (int i = 1; i <= n; i++)
    {
        QVariantMap c;
        c[QStringLiteral("id")] = QStringLiteral("camera_%1").arg(i);
        c[QStringLiteral("name")] = QStringLiteral("Cam%1").arg(i);
        c[QStringLiteral("ptz")] = QStringLiteral("false");
        cams << c;
    }
    QVariantMap home;
    home[QStringLiteral("cameras")] = cams;
    model->load(home);
    QCOMPARE(model->cameraCount(), n);
}

//Runs the event loop for ms milliseconds. The polling chain is entirely driven
//by timers and network replies, so it only progresses while the loop runs.
void TstCameraPolling::pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void TstCameraPolling::steadyStateKeepsOnePollingChainPerCamera()
{
    loadCameras(3);
    pump(300); //let the initial one-shot picture of load() settle
    server->resetRequests();

    model->set_cameraVisible(true);
    const qint64 t0 = server->now();
    pump(4000);
    const qint64 t1 = server->now();
    model->set_cameraVisible(false);

    for (int i = 1; i <= 3; i++)
    {
        const QString cam = QStringLiteral("camera_%1").arg(i);
        const QList<qint64> ts = server->stampsFor(cam);
        QVERIFY2(ts.size() >= 5, qPrintable(QStringLiteral("%1: only %2 requests")
                                            .arg(cam).arg(ts.size())));

        //One chain means the requests of a camera are serialised: the next one
        //is only armed once the previous reply came back.
        qint64 minGap = std::numeric_limits<qint64>::max();
        for (int k = 1; k < ts.size(); k++)
            minGap = qMin(minGap, ts[k] - ts[k - 1]);
        QVERIFY2(minGap >= PollIntervalMs - 40,
                 qPrintable(QStringLiteral("%1: two requests only %2 ms apart")
                            .arg(cam).arg(minGap)));

        //And the rate stays under what a single chain can produce.
        const double rate = ts.size() * 1000.0 / (t1 - t0);
        QVERIFY2(rate <= 1000.0 / PollIntervalMs + 0.5,
                 qPrintable(QStringLiteral("%1: %2 req/s").arg(cam).arg(rate)));
    }
}

void TstCameraPolling::rapidVisibilityTogglesDoNotMultiplyTheRate()
{
    loadCameras(3);
    pump(300);
    server->resetRequests();

    //--- reference window: visible, undisturbed
    model->set_cameraVisible(true);
    const qint64 refStart = server->now();
    pump(3000);
    const qint64 refEnd = server->now();

    //--- 40 visibility toggles, faster than the network round trip so that a
    //--- toggle regularly lands while a request is in flight
    int shows = 0;
    for (int i = 0; i < 40; i++)
    {
        const bool visible = (i % 2 == 1);
        if (visible)
            shows++;
        model->set_cameraVisible(visible);
        pump(25);
    }
    const qint64 burstEnd = server->now();

    //--- measurement window: visible again, undisturbed
    model->set_cameraVisible(true);
    pump(200); //let the chain pick up again
    const qint64 measStart = server->now();
    pump(3000);
    const qint64 measEnd = server->now();
    model->set_cameraVisible(false);

    for (int i = 1; i <= 3; i++)
    {
        const QString cam = QStringLiteral("camera_%1").arg(i);
        const double refRate = server->countIn(cam, refStart, refEnd) * 1000.0 / (refEnd - refStart);
        const double burstRate = server->countIn(cam, refEnd, burstEnd) * 1000.0 / (burstEnd - refEnd);
        const double measRate = server->countIn(cam, measStart, measEnd) * 1000.0 / (measEnd - measStart);

        qInfo("%s ref=%.2f req/s  burst=%.2f req/s  after=%.2f req/s",
              qPrintable(cam), refRate, burstRate, measRate);

        QVERIFY2(refRate > 1.0, qPrintable(QStringLiteral("%1: reference rate %2 req/s")
                                           .arg(cam).arg(refRate)));

        //The point of T15: no chain accumulates. Each show may cost at most one
        //immediate request (startCamera() sends one when no chain is armed),
        //on top of whatever the single running chain would have produced
        //anyway. Before T15 every show started a *new* independent chain, which
        //blows this bound apart as soon as a few toggles pile up.
        const double burstSecs = (burstEnd - refEnd) / 1000.0;
        const int burstCount = server->countIn(cam, refEnd, burstEnd);
        const int burstBudget = shows + int(refRate * burstSecs) + 2;
        QVERIFY2(burstCount <= burstBudget,
                 qPrintable(QStringLiteral("%1: %2 requests during %3 shows (budget %4)")
                            .arg(cam).arg(burstCount).arg(shows).arg(burstBudget)));

        //Nothing may be left running afterwards: once the toggling stops, the
        //rate must fall back to exactly one chain.
        QVERIFY2(measRate <= refRate * 1.3 + 0.5,
                 qPrintable(QStringLiteral("%1: %2 req/s after toggles vs %3 req/s reference")
                            .arg(cam).arg(measRate).arg(refRate)));

        //And in the quiet window after the burst the requests are serialised
        //again, which is what "one chain" means.
        QList<qint64> ts = server->stampsFor(cam);
        qint64 minGap = std::numeric_limits<qint64>::max();
        for (int k = 1; k < ts.size(); k++)
            if (ts[k] >= measStart && ts[k] <= measEnd)
                minGap = qMin(minGap, ts[k] - ts[k - 1]);
        QVERIFY2(minGap >= PollIntervalMs - 40,
                 qPrintable(QStringLiteral("%1: two requests only %2 ms apart after the burst")
                            .arg(cam).arg(minGap)));
    }
}

void TstCameraPolling::watchdogRestartsTheChainAfterALostReply()
{
    loadCameras(2);
    pump(300);
    server->swallowed.insert(QStringLiteral("camera_1"));
    server->resetRequests();

    model->set_cameraVisible(true);
    pump(1000);
    //The first request was swallowed, so the chain of camera_1 is stuck waiting.
    QCOMPARE(server->stampsFor(QStringLiteral("camera_1")).size(), 1);
    //The other camera is unaffected.
    QVERIFY(server->stampsFor(QStringLiteral("camera_2")).size() > 2);

    //The watchdog must re-issue a request once it expires.
    server->swallowed.clear();
    pump(WatchdogMs + 1000);
    QVERIFY2(server->stampsFor(QStringLiteral("camera_1")).size() > 2,
             "camera_1 never recovered from the lost reply");
    model->set_cameraVisible(false);
}

void TstCameraPolling::imageCacheSurvivesConcurrentProviderReads()
{
    //Direct test of the primitive T15 introduced: the provider reads the cache
    //from a Qt Quick image loading thread while the GUI thread writes it.
    ImageCachePtr cache = ImageCachePtr::create();
    ModelImageProvider provider(cache);

    QImage img(8, 8, QImage::Format_RGB32);
    img.fill(Qt::red);
    for (int i = 0; i < 8; i++)
        cache->setImage(QStringLiteral("cam%1").arg(i), img);

    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};
    QList<QThread *> threads;
    for (int t = 0; t < 4; t++)
    {
        QThread *th = QThread::create([&provider, &stop, &reads]()
        {
            while (!stop.load())
            {
                for (int i = 0; i < 8; i++)
                {
                    QSize s;
                    QImage got = provider.requestImage(QStringLiteral("cam%1/12345").arg(i),
                                                       &s, QSize(4, 4));
                    Q_UNUSED(got)
                    reads++;
                }
            }
        });
        th->start();
        threads << th;
    }

    QElapsedTimer t;
    t.start();
    int gen = 0;
    while (t.elapsed() < 1500)
    {
        QImage w(8, 8, QImage::Format_RGB32);
        w.fill(QColor(gen % 255, 0, 0));
        for (int i = 0; i < 8; i++)
            cache->setImage(QStringLiteral("cam%1").arg(i), w);
        if (gen % 50 == 0)
            cache->removeImage(QStringLiteral("cam3"));
        if (gen % 200 == 0)
            cache->clear();
        gen++;
    }
    stop = true;
    for (QThread *th: threads)
    {
        QVERIFY(th->wait(5000));
        delete th;
    }
    QVERIFY(reads.load() > 0);
}

int main(int argc, char *argv[])
{
    //Headless by construction: the test never shows a window, and CI has no
    //display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TstCameraPolling tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_camerapolling.moc"
