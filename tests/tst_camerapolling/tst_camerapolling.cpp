/*
 * Tests unitaires de src/CameraModel.cpp — cadence de polling des cameras (T15).
 *
 * La demande vient des vues : chaque vue declare un CameraFeed, et une camera
 * sans feed actif n'envoie aucune requete. Les pages QML reelles sont chargees
 * ici pour le prouver, hors ecran et vue quittee compris.
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
 * l'interdisent : startCamera() respecte un plancher temporel base sur la date
 * de la derniere requete, donc reafficher la camera plus vite que l'intervalle
 * de polling ne declenche pas de requete immediate. Le debit pendant une rafale
 * de navigation reste borne par l'intervalle de polling et non par la cadence
 * de navigation - c'est ce que verifie burstBudget ci-dessous. Avant T15 chaque
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
#include <QBuffer>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include "CalaosConnection.h"
#include "CameraFrameDecoder.h"
#include "CameraModel.h"
#include "Common.h"
#include "ModelImageProvider.h"

//Intervalle nominal entre deux requetes d'une meme camera (CameraModel.cpp).
static const int PollIntervalMs = 200;
//Chien de garde de CameraItem (CameraModel.cpp).
static const int WatchdogMs = 5000;
//Delai raccourci et transfert lent qui le depasse trois fois : c'est le rapport
//entre les deux qui compte, pas les 5 s.
static const int ShortWatchdogMs = 600;
static const int SlowChunks = 20;
static const int SlowChunkMs = 100;
static const int SlowTransferMs = SlowChunks * SlowChunkMs;

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
    //Corps servi pour get_picture, a la place du petit JPEG par defaut.
    QByteArray frame;
    //Lien lent : la reponse part en SlowChunks morceaux, un tous les SlowChunkMs.
    bool slowLink = false;
    //Cameras dont le transfert lent se fige a mi-chemin, connexion ouverte.
    QSet<QString> stalled;

    //Horodatage (ms depuis le demarrage du serveur) de chaque requete recue.
    QMultiHash<QString, qint64> requests;
    //Requetes d'image dont le client tient encore la connexion ouverte.
    QHash<QTcpSocket *, QString> openPictures;

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

    QStringList polledCameras() const
    {
        QStringList l = requests.uniqueKeys();
        l.sort();
        return l;
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
        connect(sock, &QTcpSocket::disconnected, this, [this, sock, buf]()
        {
            openPictures.remove(sock);
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
            openPictures.insert(sock, camId);
            if (swallowed.contains(camId))
                return; //never answer: exercises the watchdog
            QTimer::singleShot(replyDelayMs, sock, [this, sock, camId]()
            {
                if (sock->state() != QAbstractSocket::ConnectedState)
                    return;
                const QByteArray picture = frame.isEmpty() ? jpeg() : frame;
                if (slowLink)
                    sendChunk(sock, camId, picture, 0);
                else
                    reply(sock, "image/jpeg", picture);
            });
            return;
        }

        //Anything else (get_home, poll_listen, ...) gets a harmless answer.
        reply(sock, "application/json",
              QByteArray("{\"success\":\"true\",\"home\":[],\"cameras\":[],\"audio\":[]}"));
    }

    static QByteArray headers(const char *mime, qsizetype size)
    {
        QByteArray out = "HTTP/1.1 200 OK\r\nContent-Type: ";
        out += mime;
        out += "\r\nContent-Length: " + QByteArray::number(size);
        out += "\r\nConnection: close\r\n\r\n";
        return out;
    }

    static void reply(QTcpSocket *sock, const char *mime, const QByteArray &body)
    {
        sock->write(headers(mime, body.size()) + body);
        sock->flush();
        sock->disconnectFromHost();
    }

    void sendChunk(QTcpSocket *sock, const QString &camId, const QByteArray &body, int chunk)
    {
        const qsizetype chunkSize = body.size() / SlowChunks + 1;
        const qsizetype sent = chunk * chunkSize;
        if (chunk == 0)
            sock->write(headers("image/jpeg", body.size()));
        sock->write(body.mid(sent, chunkSize));
        sock->flush();

        if (sent + chunkSize >= body.size())
        {
            sock->disconnectFromHost();
            return;
        }
        if (chunk + 1 == SlowChunks / 2 && stalled.contains(camId))
            return;
        QTimer::singleShot(SlowChunkMs, sock, [this, sock, camId, body, chunk]()
        {
            sendChunk(sock, camId, body, chunk + 1);
        });
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

//What the application exposes to the camera pages, reduced to what they read.
class FakeCalaosApp: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE QString getPictureSized(const QString &) const { return QString(); }
};

//Same size and weight as a real camera frame: 4512x2512, about 860 KB. Blocks of
//noise rather than a flat color, otherwise the JPEG weighs next to nothing and
//the decoder has no work to do.
static QByteArray syntheticFrame()
{
    static QByteArray data;
    if (data.isEmpty())
    {
        QImage img(4512, 2512, QImage::Format_RGB32);
        QRandomGenerator rng(4512);
        const int block = 8;
        for (int y = 0; y < img.height(); y += block)
        {
            for (int x = 0; x < img.width(); x += block)
            {
                const QRgb color = qRgb(rng.bounded(256), rng.bounded(256), rng.bounded(256));
                const int yEnd = qMin(y + block, img.height());
                const int xEnd = qMin(x + block, img.width());
                for (int by = y; by < yEnd; by++)
                {
                    QRgb *line = reinterpret_cast<QRgb *>(img.scanLine(by));
                    std::fill(line + x, line + xEnd, color);
                }
            }
        }
        QBuffer b(&data);
        b.open(QIODevice::WriteOnly);
        img.save(&b, "JPEG", 45);
    }
    return data;
}

class TstCameraPolling: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    //Le decodage se fait hors du fil appelant, reduit a la taille demandee.
    void decoderRunsOutsideTheCallingThread();
    void decoderReducesToTheRequestedSize();
    void decoderReportsABrokenFrame();

    //Une camera sans feed actif ne coute aucune requete.
    void loadingCamerasSendsNoRequest();
    void onlyCamerasWithAnActiveFeedArePolled();
    void hidingACameraCancelsItsRequest();
    void frameDecodedAfterHidingIsDropped();
    void destroyingTheLastFeedStopsTheCamera();
    void reloadingForgetsTheDeletedCameras();
    void backgroundPausesThePolling();

    //Un serveur lent ne recoit jamais qu'une requete par camera.
    void slowServerGetsOneRequestAtATime();
    void watchdogReplacesTheLostRequest();

    //Le chien de garde mesure le silence, pas la duree du transfert.
    void slowTransferIsShownOnceComplete();
    void silentRequestIsReplacedAndTheCameraRecovers();
    void stalledTransferIsReplacedAfterItsLastByte();
    void progressOfAnotherCameraDoesNotHoldTheWatchdog();
    void moreCamerasThanConnectionsAllGetAFrame();
    void slowTransferOfADeletedCameraStartsNothing();

    //Plusieurs vues sur une camera : la plus exigeante decide.
    void pollingFollowsTheFeedInterval();
    void shortestFeedIntervalWins();
    void decodedFrameFitsTheDisplay();
    void largestFeedDecidesTheDecodedSize();
    void feedWithoutSizeGetsTheFullFrame();

    //Une frame illisible ne casse ni l'image affichee ni la chaine.
    void brokenFrameKeepsTheLastImage();

    //Pages QML reelles : ni hors ecran, ni vue quittee.
    void mobileListPollsOnlyTheCamerasOnScreen();
    void mobileListStopsWhenLeft();
    void mobileSingleViewPollsItsCameraAlone();
    void desktopListPollsOnlyTheCurrentPage();
    void desktopListStopsWhenLeft();
    void desktopSingleViewPollsItsCameraAlone();

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
    void showCameras(bool visible);
    void pump(int ms);
    CameraFeed *addFeed(int camera, const QSize &frameSize = QSize(), int interval = 0);
    CameraItem *cameraAt(int idx);
    QImage shownImage(int camera);
    void loadScene(const QByteArray &variant, const QByteArray &page);
    QQuickItem *sceneList() const;
    QStringList camerasOnScreen(int camerasPerDelegate) const;
    int aliveOffScreen() const;
    void requestsOverAWindow(int ms);
    void shortenWatchdogs();

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    CameraModel *model = nullptr;
    FakeApiServer *server = nullptr;
    FakeCalaosApp fakeApp;
    QList<CameraFeed *> feeds;
    QQuickWindow *scene = nullptr;
};

void TstCameraPolling::initTestCase()
{
    Common::registerQml();
    qmlRegisterType<CameraFeed>("Calaos", 1, 0, "CameraFeed");
}

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
    delete scene;
    scene = nullptr;
    qDeleteAll(feeds);
    feeds.clear();
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

    for (int i = 0; i < n; i++)
        addFeed(i);
}

//What a view does: one feed per camera, active while the camera is on screen.
void TstCameraPolling::showCameras(bool visible)
{
    for (CameraFeed *feed: std::as_const(feeds))
        feed->setActive(visible);
}

//Runs the event loop for ms milliseconds. The polling chain is entirely driven
//by timers and network replies, so it only progresses while the loop runs.
void TstCameraPolling::pump(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

CameraFeed *TstCameraPolling::addFeed(int camera, const QSize &frameSize, int interval)
{
    CameraFeed *feed = new CameraFeed();
    feed->setCamera(model->getItemModel(camera));
    feed->setFrameSize(frameSize);
    if (interval > 0)
        feed->setInterval(interval);
    feeds << feed;
    return feed;
}

CameraItem *TstCameraPolling::cameraAt(int idx)
{
    return qobject_cast<CameraItem *>(model->getItemModel(idx));
}

//What the QML Image of this camera gets from the provider.
QImage TstCameraPolling::shownImage(int camera)
{
    auto *provider = static_cast<QQuickImageProvider *>(
        engine->imageProvider(QStringLiteral("camera")));
    QSize size;
    return provider->requestImage(cameraAt(camera)->get_cameraId() + QStringLiteral("/1"),
                                  &size, QSize());
}

//The real page in a StackView, as main.qml pushes it. The scene claims to live
//in the page's folder so that the sibling components of the page resolve.
void TstCameraPolling::loadScene(const QByteArray &variant, const QByteArray &page)
{
    engine->addImportPath(QStringLiteral(CALAOS_QML_DIR));
    QQmlContext *ctx = engine->rootContext();
    ctx->setContextProperty(QStringLiteral("cameraModel"), model);
    ctx->setContextProperty(QStringLiteral("calaosApp"), &fakeApp);
    ctx->setContextProperty(QStringLiteral("isLandscape"), false);
    ctx->setContextProperty(QStringLiteral("calaosFont"), QVariantMap {
        { QStringLiteral("fontFamily"), QStringLiteral("sans") },
        { QStringLiteral("fontFamilyLight"), QStringLiteral("sans") },
    });

    const QByteArray cameraProperty = variant == "mobile" ? "modelData" : "camModel";
    const QByteArray qml =
        "import QtQuick\n"
        "import QtQuick.Controls as Controls\n"
        "Window {\n"
        "    width: 1024; height: 600; visible: true\n"
        "    property var currentCameraModel: null\n"
        "    function leave() { stack.push(other) }\n"
        "    function back() { stack.pop() }\n"
        "    function openCamera(idx) {\n"
        "        currentCameraModel = cameraModel.getItemModel(idx)\n"
        "        stack.push(single)\n"
        "    }\n"
        "    Component { id: other; Item {} }\n"
        "    Component { id: single; CameraSingleView { " + cameraProperty + ": currentCameraModel } }\n"
        "    Component { id: start; " + page + " {} }\n"
        "    Controls.StackView {\n"
        "        id: stack\n"
        "        anchors.fill: parent\n"
        "        initialItem: start\n"
        "        pushEnter: Transition {}\n"
        "        pushExit: Transition {}\n"
        "        popEnter: Transition {}\n"
        "        popExit: Transition {}\n"
        "    }\n"
        "}\n";

    QQmlComponent component(engine);
    component.setData(qml, QUrl::fromLocalFile(QStringLiteral(CALAOS_QML_DIR "/") +
                                               QString::fromLatin1(variant) +
                                               QStringLiteral("/tst_scene.qml")));
    scene = qobject_cast<QQuickWindow *>(component.create());
    QVERIFY2(scene, qPrintable(component.errorString()));
}

static QQuickItem *findListView(QQuickItem *root)
{
    if (root->inherits("QQuickListView"))
        return root;

    const QList<QQuickItem *> children = root->childItems();
    for (QQuickItem *child: children)
    {
        if (QQuickItem *found = findListView(child))
            return found;
    }
    return nullptr;
}

QQuickItem *TstCameraPolling::sceneList() const
{
    return findListView(scene->contentItem());
}

static QQuickItem *delegateAt(QQuickItem *list, int idx)
{
    QQuickItem *item = nullptr;
    QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, item), Q_ARG(int, idx));
    return item;
}

static bool crossesViewport(QQuickItem *list, QQuickItem *delegate)
{
    const bool vertical = list->property("orientation").toInt() == Qt::Vertical;
    const qreal pos = vertical ? delegate->y() : delegate->x();
    const qreal size = vertical ? delegate->height() : delegate->width();
    const qreal first = list->property(vertical ? "contentY" : "contentX").toReal();
    const qreal length = vertical ? list->height() : list->width();
    return pos + size > first && pos < first + length;
}

//The ids the current scroll position makes visible, from the delegates of the
//real ListView: camerasPerDelegate is 1 for a mobile line, 4 for a desktop page.
QStringList TstCameraPolling::camerasOnScreen(int camerasPerDelegate) const
{
    QQuickItem *list = sceneList();
    QStringList ids;
    const int delegates = list->property("count").toInt();
    for (int i = 0; i < delegates; i++)
    {
        QQuickItem *delegate = delegateAt(list, i);
        if (!delegate || !crossesViewport(list, delegate))
            continue;
        for (int c = 0; c < camerasPerDelegate; c++)
        {
            const int cam = i * camerasPerDelegate + c;
            if (cam < model->cameraCount())
                ids << QStringLiteral("camera_%1").arg(cam + 1);
        }
    }
    ids.sort();
    return ids;
}

//Delegates kept alive past the edges: without any, the off screen tests prove nothing.
int TstCameraPolling::aliveOffScreen() const
{
    QQuickItem *list = sceneList();
    int n = 0;
    const int delegates = list->property("count").toInt();
    for (int i = 0; i < delegates; i++)
    {
        QQuickItem *delegate = delegateAt(list, i);
        if (delegate && !crossesViewport(list, delegate))
            n++;
    }
    return n;
}

//Lets what was already asked land, then records a clean window of requests.
void TstCameraPolling::requestsOverAWindow(int ms)
{
    pump(150);
    server->resetRequests();
    pump(ms);
}

void TstCameraPolling::shortenWatchdogs()
{
    for (int i = 0; i < model->cameraCount(); i++)
        cameraAt(i)->setWatchdogInterval(ShortWatchdogMs);
}

void TstCameraPolling::decoderRunsOutsideTheCallingThread()
{
    CameraFrameDecoder decoder;
    QThread *worker = nullptr;
    bool done = false;
    connect(&decoder, &CameraFrameDecoder::decoded, this, [&](quint64, const QImage &)
    {
        worker = QThread::currentThread();
        done = true;
    }, Qt::DirectConnection);

    decoder.submit(syntheticFrame(), QSize(236, 131));
    QTRY_VERIFY(done);
    QVERIFY(worker != QThread::currentThread());
}

void TstCameraPolling::decoderReducesToTheRequestedSize()
{
    const QByteArray frame = syntheticFrame();
    //Same size and weight as a real frame, see syntheticFrame()
    QVERIFY2(frame.size() > 700 * 1024 && frame.size() < 1024 * 1024,
             qPrintable(QString::number(frame.size())));

    QCOMPARE(CameraFrameDecoder::decode(frame, QSize()).size(), QSize(4512, 2512));

    //The aspect ratio of the source wins over the one of the box
    const QImage reduced = CameraFrameDecoder::decode(frame, QSize(236, 174));
    QCOMPARE(reduced.width(), 236);
    QCOMPARE(reduced.height(), qRound(236.0 * 2512 / 4512));

    //Never upscaled
    QCOMPARE(CameraFrameDecoder::decode(frame, QSize(8000, 8000)).size(), QSize(4512, 2512));

    //Not every camera serves JPEG
    QImage small(40, 20, QImage::Format_RGB32);
    small.fill(Qt::blue);
    QByteArray png;
    QBuffer b(&png);
    b.open(QIODevice::WriteOnly);
    QVERIFY(small.save(&b, "PNG"));
    QCOMPARE(CameraFrameDecoder::decode(png, QSize(20, 20)).size(), QSize(20, 10));
}

void TstCameraPolling::decoderReportsABrokenFrame()
{
    QVERIFY(CameraFrameDecoder::decode(QByteArray("not a jpeg"), QSize(100, 100)).isNull());
    QVERIFY(CameraFrameDecoder::decode(QByteArray(), QSize()).isNull());
}

void TstCameraPolling::loadingCamerasSendsNoRequest()
{
    server->resetRequests();
    loadCameras(6);
    pump(600);
    QCOMPARE(server->polledCameras(), QStringList());
}

void TstCameraPolling::onlyCamerasWithAnActiveFeedArePolled()
{
    loadCameras(3);
    feeds.at(1)->setActive(true);
    pump(600);
    QCOMPARE(server->polledCameras(), QStringList { QStringLiteral("camera_2") });

    feeds.at(1)->setActive(false);
    requestsOverAWindow(600);
    QCOMPARE(server->polledCameras(), QStringList());
}

void TstCameraPolling::hidingACameraCancelsItsRequest()
{
    loadCameras(1);
    server->replyDelayMs = 400;
    QSignalSpy downloaded(connection, &CalaosConnection::cameraPictureDownloaded);

    feeds.at(0)->setActive(true);
    pump(150);
    QCOMPARE(server->openPictures.size(), 1);

    feeds.at(0)->setActive(false);
    //The socket goes at once, the server does not have to finish its reply
    QTRY_COMPARE_WITH_TIMEOUT(server->openPictures.size(), 0, 200);
    pump(400);
    QCOMPARE(downloaded.size(), 0);
}

void TstCameraPolling::frameDecodedAfterHidingIsDropped()
{
    loadCameras(1);
    const QString before = cameraAt(0)->get_url_single();
    server->frame = syntheticFrame();

    //Runs right after CameraItem handed the frame to the decoder, and on the GUI
    //thread, so before the result can possibly be delivered.
    bool hidden = false;
    connect(connection, &CalaosConnection::cameraPictureDownloaded, this, [&]()
    {
        hidden = true;
        feeds.at(0)->setActive(false);
    });

    feeds.at(0)->setActive(true);
    QTRY_VERIFY(hidden);
    pump(300);
    QCOMPARE(cameraAt(0)->get_url_single(), before);
}

void TstCameraPolling::destroyingTheLastFeedStopsTheCamera()
{
    loadCameras(1);
    CameraFeed *feed = feeds.takeFirst();
    feed->setActive(true);
    pump(300);
    QVERIFY(!server->requests.isEmpty());

    delete feed;
    QVERIFY(!cameraAt(0)->get_cameraVisible());
    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras(), QStringList());
}

void TstCameraPolling::reloadingForgetsTheDeletedCameras()
{
    loadCameras(2);
    CameraFeed *stale = feeds.at(0);
    showCameras(true);
    pump(300);

    //A reload deletes the items under the feeds the pages still hold
    loadCameras(2);
    QCOMPARE(stale->camera(), nullptr);

    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras(), QStringList());
}

void TstCameraPolling::backgroundPausesThePolling()
{
    loadCameras(2);
    showCameras(true);
    pump(300);

    auto setState = [this](Qt::ApplicationState state)
    {
        QVERIFY(QMetaObject::invokeMethod(model, "applicationStateChanged",
                                          Q_ARG(Qt::ApplicationState, state)));
    };

    setState(Qt::ApplicationSuspended);
    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras(), QStringList());

    //Loaded while in background: a reconnection can do this
    qDeleteAll(feeds);
    feeds.clear();
    loadCameras(2);
    showCameras(true);
    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras(), QStringList());

    setState(Qt::ApplicationActive);
    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras().size(), 2);

    //The notification shade of a phone, and always the case on a kiosk without
    //a window manager
    setState(Qt::ApplicationInactive);
    requestsOverAWindow(500);
    QCOMPARE(server->polledCameras().size(), 2);
}

void TstCameraPolling::slowServerGetsOneRequestAtATime()
{
    loadCameras(6);
    server->replyDelayMs = 900;
    showCameras(true);

    int maxOpen = 0;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 3000)
    {
        pump(20);
        QHash<QString, int> open;
        for (const QString &cam: std::as_const(server->openPictures))
            maxOpen = qMax(maxOpen, ++open[cam]);
    }
    QCOMPARE(maxOpen, 1);

    for (int i = 1; i <= 6; i++)
    {
        const QList<qint64> ts = server->stampsFor(QStringLiteral("camera_%1").arg(i));
        QVERIFY2(!ts.isEmpty(), "camera never polled");
        for (int k = 1; k < ts.size(); k++)
            QVERIFY(ts[k] - ts[k - 1] >= server->replyDelayMs);
    }
}

void TstCameraPolling::watchdogReplacesTheLostRequest()
{
    loadCameras(1);
    server->swallowed.insert(QStringLiteral("camera_1"));
    showCameras(true);

    pump(WatchdogMs + 500);
    QCOMPARE(server->stampsFor(QStringLiteral("camera_1")).size(), 2);

    //Left open, each lost reply would hold one of the six connections per host
    //of QNetworkAccessManager until none is left for the other cameras
    QTRY_COMPARE_WITH_TIMEOUT(server->openPictures.size(), 1, 200);
}

void TstCameraPolling::slowTransferIsShownOnceComplete()
{
    const QString cam = QStringLiteral("camera_1");
    loadCameras(1);
    shortenWatchdogs();
    server->slowLink = true;
    showCameras(true);

    QTRY_VERIFY_WITH_TIMEOUT(shownImage(0).size() == QSize(2, 2), 3 * SlowTransferMs);

    const QList<qint64> ts = server->stampsFor(cam);
    QCOMPARE(ts.size(), 1);
    const qint64 transfer = server->now() - ts.first();
    QVERIFY2(transfer >= 3 * ShortWatchdogMs, qPrintable(QString::number(transfer)));
}

void TstCameraPolling::silentRequestIsReplacedAndTheCameraRecovers()
{
    const QString cam = QStringLiteral("camera_1");
    loadCameras(1);
    shortenWatchdogs();
    server->swallowed.insert(cam);
    showCameras(true);

    QTRY_COMPARE_WITH_TIMEOUT(server->stampsFor(cam).size(), 2, 3 * ShortWatchdogMs);
    const QList<qint64> ts = server->stampsFor(cam);
    QVERIFY(ts[1] - ts[0] >= ShortWatchdogMs / 2);
    QTRY_COMPARE_WITH_TIMEOUT(server->openPictures.size(), 1, 200);

    server->swallowed.clear();
    QTRY_VERIFY_WITH_TIMEOUT(shownImage(0).size() == QSize(2, 2), 3 * ShortWatchdogMs);
}

void TstCameraPolling::stalledTransferIsReplacedAfterItsLastByte()
{
    const QString cam = QStringLiteral("camera_1");
    loadCameras(1);
    shortenWatchdogs();
    server->slowLink = true;
    server->stalled.insert(cam);
    showCameras(true);

    QTRY_COMPARE_WITH_TIMEOUT(server->stampsFor(cam).size(), 2, 2 * SlowTransferMs);

    //The bytes stop half way through: the delay counts from there, not from the
    //request, and the bytes do not switch the watchdog off either. Half a delay
    //of margin: the timer is coarse and the last chunk may raise no progress.
    const QList<qint64> ts = server->stampsFor(cam);
    const qint64 lastByte = (SlowChunks / 2 - 1) * SlowChunkMs;
    QVERIFY2(ts[1] - ts[0] >= lastByte + ShortWatchdogMs / 2,
             qPrintable(QString::number(ts[1] - ts[0])));

    server->stalled.clear();
    QTRY_VERIFY_WITH_TIMEOUT(shownImage(0).size() == QSize(2, 2), 3 * SlowTransferMs);
}

void TstCameraPolling::progressOfAnotherCameraDoesNotHoldTheWatchdog()
{
    const QString lost = QStringLiteral("camera_1");
    loadCameras(2);
    shortenWatchdogs();
    server->slowLink = true;
    server->swallowed.insert(lost);
    showCameras(true);

    QTRY_COMPARE_WITH_TIMEOUT(server->stampsFor(lost).size(), 2, 2 * ShortWatchdogMs);
}

//Six connections per host in QNetworkAccessManager: the last two cameras wait in
//its queue, with no byte to show, while the first six take longer than the delay.
void TstCameraPolling::moreCamerasThanConnectionsAllGetAFrame()
{
    loadCameras(8);
    shortenWatchdogs();
    server->slowLink = true;
    showCameras(true);

    for (int i = 0; i < 8; i++)
        QTRY_VERIFY_WITH_TIMEOUT(shownImage(i).size() == QSize(2, 2), 5 * SlowTransferMs);
}

void TstCameraPolling::slowTransferOfADeletedCameraStartsNothing()
{
    loadCameras(1);
    server->slowLink = true;
    showCameras(true);
    QTRY_COMPARE(server->requests.size(), 1);

    //Same id, nobody watching: the bytes of the old reply concern it no more
    loadCameras(1);
    shortenWatchdogs();
    pump(SlowTransferMs + 2 * ShortWatchdogMs);
    QCOMPARE(server->requests.size(), 1);
}

void TstCameraPolling::pollingFollowsTheFeedInterval()
{
    loadCameras(1);
    feeds.at(0)->setInterval(500);
    feeds.at(0)->setActive(true);
    pump(2200);

    const QList<qint64> ts = server->stampsFor(QStringLiteral("camera_1"));
    QVERIFY2(ts.size() >= 3, qPrintable(QString::number(ts.size())));
    for (int k = 1; k < ts.size(); k++)
        QVERIFY(ts[k] - ts[k - 1] >= 500 - 40);
}

void TstCameraPolling::shortestFeedIntervalWins()
{
    loadCameras(1);
    addFeed(0, QSize(), 500)->setActive(true);
    addFeed(0, QSize(), PollIntervalMs)->setActive(true);
    pump(1500);
    QVERIFY(server->stampsFor(QStringLiteral("camera_1")).size() >= 5);

    //A feed cannot go under the floor of the single view
    CameraFeed *greedy = addFeed(0);
    greedy->setInterval(10);
    QCOMPARE(greedy->interval(), PollIntervalMs);
}

void TstCameraPolling::decodedFrameFitsTheDisplay()
{
    loadCameras(1);
    server->frame = syntheticFrame();
    addFeed(0, QSize(236, 174))->setActive(true);

    QTRY_VERIFY(shownImage(0).width() == 236);
    QCOMPARE(shownImage(0).height(), qRound(236.0 * 2512 / 4512));
}

void TstCameraPolling::largestFeedDecidesTheDecodedSize()
{
    loadCameras(1);
    server->frame = syntheticFrame();
    addFeed(0, QSize(236, 174))->setActive(true);
    addFeed(0, QSize(640, 480))->setActive(true);

    QTRY_VERIFY(shownImage(0).width() == 640);
}

void TstCameraPolling::feedWithoutSizeGetsTheFullFrame()
{
    loadCameras(1);
    server->frame = syntheticFrame();
    addFeed(0, QSize(236, 174))->setActive(true);
    addFeed(0)->setActive(true);

    QTRY_VERIFY(shownImage(0).width() == 4512);
}

void TstCameraPolling::brokenFrameKeepsTheLastImage()
{
    loadCameras(1);
    showCameras(true);
    QTRY_VERIFY(shownImage(0).size() == QSize(2, 2));
    const QString lastGood = cameraAt(0)->get_url_single();

    server->frame = QByteArrayLiteral("not a jpeg");
    requestsOverAWindow(600);
    QCOMPARE(cameraAt(0)->get_url_single(), lastGood);
    QCOMPARE(shownImage(0).size(), QSize(2, 2));
    QVERIFY2(server->requests.size() >= 2, "a broken frame stopped the chain");
}

void TstCameraPolling::mobileListPollsOnlyTheCamerasOnScreen()
{
    loadCameras(30);
    loadScene("mobile", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());

    requestsOverAWindow(800);
    QVERIFY(aliveOffScreen() > 0);
    QStringList expected = camerasOnScreen(1);
    QVERIFY(!expected.isEmpty());
    QCOMPARE(server->polledCameras(), expected);

    QQuickItem *list = sceneList();
    list->setProperty("contentY", list->height() * 2);
    requestsOverAWindow(800);
    const QStringList scrolled = camerasOnScreen(1);
    QVERIFY(scrolled != expected);
    QVERIFY(aliveOffScreen() > 0);
    QCOMPARE(server->polledCameras(), scrolled);
}

void TstCameraPolling::mobileListStopsWhenLeft()
{
    loadCameras(6);
    loadScene("mobile", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());
    requestsOverAWindow(800);
    QVERIFY(!server->polledCameras().isEmpty());

    QVERIFY(QMetaObject::invokeMethod(scene, "leave"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras(), QStringList());

    QVERIFY(QMetaObject::invokeMethod(scene, "back"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras(), camerasOnScreen(1));
}

void TstCameraPolling::mobileSingleViewPollsItsCameraAlone()
{
    loadCameras(6);
    loadScene("mobile", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());

    QVERIFY(QMetaObject::invokeMethod(scene, "openCamera", Q_ARG(QVariant, 2)));
    requestsOverAWindow(1000);
    QCOMPARE(server->polledCameras(), QStringList { QStringLiteral("camera_3") });
    //The rate of the single view, not the one of the thumbnails
    QVERIFY(server->requests.size() >= 3);

    QVERIFY(QMetaObject::invokeMethod(scene, "back"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras(), camerasOnScreen(1));
}

void TstCameraPolling::desktopListPollsOnlyTheCurrentPage()
{
    loadCameras(16);
    loadScene("desktop", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());

    requestsOverAWindow(800);
    QVERIFY(aliveOffScreen() > 0);
    QCOMPARE(camerasOnScreen(4).size(), 4);
    QCOMPARE(server->polledCameras(), camerasOnScreen(4));

    QQuickItem *list = sceneList();
    list->setProperty("contentX", delegateAt(list, 1)->x());
    requestsOverAWindow(800);
    QCOMPARE(camerasOnScreen(4).first(), QStringLiteral("camera_5"));
    QCOMPARE(server->polledCameras(), camerasOnScreen(4));
}

void TstCameraPolling::desktopListStopsWhenLeft()
{
    loadCameras(4);
    loadScene("desktop", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras().size(), 4);

    QVERIFY(QMetaObject::invokeMethod(scene, "leave"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras(), QStringList());

    QVERIFY(QMetaObject::invokeMethod(scene, "back"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras().size(), 4);
}

void TstCameraPolling::desktopSingleViewPollsItsCameraAlone()
{
    loadCameras(4);
    loadScene("desktop", "CameraListView");
    QVERIFY(!QTest::currentTestFailed());

    QVERIFY(QMetaObject::invokeMethod(scene, "openCamera", Q_ARG(QVariant, 2)));
    requestsOverAWindow(1000);
    QCOMPARE(server->polledCameras(), QStringList { QStringLiteral("camera_3") });
    QVERIFY(server->requests.size() >= 3);

    QVERIFY(QMetaObject::invokeMethod(scene, "back"));
    requestsOverAWindow(800);
    QCOMPARE(server->polledCameras().size(), 4);
}

void TstCameraPolling::steadyStateKeepsOnePollingChainPerCamera()
{
    loadCameras(3);

    showCameras(true);
    const qint64 t0 = server->now();
    pump(4000);
    const qint64 t1 = server->now();
    showCameras(false);

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

    //--- reference window: visible, undisturbed
    showCameras(true);
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
        showCameras(visible);
        pump(25);
    }
    const qint64 burstEnd = server->now();

    //--- measurement window: visible again, undisturbed
    showCameras(true);
    pump(200); //let the chain pick up again
    const qint64 measStart = server->now();
    pump(3000);
    const qint64 measEnd = server->now();
    showCameras(false);

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
        //Le plancher temporel de startCamera() interdit desormais la requete
        //immediate a l'entree : une rafale ne peut plus depasser ce que
        //l'intervalle de polling autorise, quel que soit le rythme de
        //navigation. Le budget ne depend donc plus du nombre d'affichages.
        const int burstBudget = int(refRate * burstSecs) + 2;
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
    server->swallowed.insert(QStringLiteral("camera_1"));

    showCameras(true);
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
    showCameras(false);
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
    //Headless by construction: CI has no display and no GPU for the windows of
    //the camera pages.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    QApplication app(argc, argv);
    TstCameraPolling tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_camerapolling.moc"
