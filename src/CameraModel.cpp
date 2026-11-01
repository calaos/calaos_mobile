#include "CameraModel.h"
#include "CameraFrameDecoder.h"
#include "ScreenManager.h"
#include "JsonKeys.h"
#include <QGuiApplication>
#include <limits>
#include <qfappdispatcher.h>

//Delay between two picture requests of a visible camera
static const int CameraPollIntervalMs = 200;
//Safety net: a request silent for this long is lost, retry instead of freezing
//the camera forever. Every received byte restarts it: a slow link must get its
//frame late rather than never.
static const int CameraPollWatchdogMs = 5000;

CameraFeed::CameraFeed(QObject *parent):
    QObject(parent),
    intervalMs(CameraPollIntervalMs)
{
}

CameraFeed::~CameraFeed()
{
    if (item && feedActive)
        item->removeFeed(this);
}

QObject *CameraFeed::camera() const
{
    return item.data();
}

void CameraFeed::setCamera(QObject *camera)
{
    CameraItem *next = qobject_cast<CameraItem *>(camera);
    if (item == next)
        return;

    if (item && feedActive)
        item->removeFeed(this);
    item = next;
    if (item && feedActive)
        item->addFeed(this);

    emit cameraChanged();
}

void CameraFeed::setActive(bool active)
{
    if (feedActive == active)
        return;

    feedActive = active;
    if (item)
    {
        if (feedActive)
            item->addFeed(this);
        else
            item->removeFeed(this);
    }

    emit activeChanged();
}

void CameraFeed::setFrameSize(const QSize &frameSize)
{
    if (size == frameSize)
        return;

    size = frameSize;
    emit frameSizeChanged();
}

void CameraFeed::setInterval(int interval)
{
    const int ms = qMax(interval, CameraPollIntervalMs);
    if (intervalMs == ms)
        return;

    intervalMs = ms;
    emit intervalChanged();
}

CameraModel::CameraModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con),
    decoder(new CameraFrameDecoder(this))
{
    QHash<int, QByteArray> roles;
    roles[RoleId] = "cameraId";
    roles[RoleName] = "name";
    roles[RoleUrl] = "url_single";
    roles[RolePTZ] = "hasPTZ";
    setItemRoleNames(roles);

    //add a special image provider for single pictures of cameras
    //The cache is shared with the items, the provider itself is owned by the engine
    imageCache = ImageCachePtr::create();
    imgProvider = new CameraImageProvider(imageCache);
    engine->addImageProvider(QLatin1String("camera"), imgProvider);

    connect(connection, &CalaosConnection::eventTouchscreenCamera,
            this, &CameraModel::eventTouchscreenCamera);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged,
            this, &CameraModel::applicationStateChanged);
}

void CameraModel::applicationStateChanged(Qt::ApplicationState state)
{
    //Not "anything but active": the kiosk runs without a window manager and
    //may never become ApplicationActive, and a phone goes through Inactive for
    //a notification shade that leaves the view on screen.
    inBackground = state == Qt::ApplicationSuspended || state == Qt::ApplicationHidden;

    for (int i = 0;i < rowCount();i++)
    {
        CameraItem *cam = dynamic_cast<CameraItem *>(item(i));
        if (cam)
            cam->setInBackground(inBackground);
    }
}

void CameraModel::load(const QVariantMap &homeData)
{
    clear();
    imageCache->clear();

    if (!homeData.contains(JsonKeys::Cameras))
    {
        qDebug() << "no camera entry";
        return;
    }

    QVariantList cameras = homeData[JsonKeys::Cameras].toList();
    QVariantList::iterator it = cameras.begin();
    for (int i = 0;it != cameras.end();it++, i++)
    {
        QVariantMap r = it->toMap();
        CameraItem *p = new CameraItem(connection, imageCache, decoder);
        p->setInBackground(inBackground);
        p->load(r, i);
        appendRow(p);
    }
}

void CameraModel::eventTouchscreenCamera(QString cameraId)
{
    //lookup for camera in our model
    for (int i = 0;i < rowCount();i++)
    {
        CameraItem *cam = dynamic_cast<CameraItem *>(item(i));
        if (cam && cam->get_cameraId() == cameraId)
        {
            //Wake up screen
            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);

#ifdef CALAOS_DESKTOP
            ScreenManager::Instance().wakeupScreen();

            //Also tell ScreenSuspend.qml to wake up
            appDispatcher->dispatch("wakeupScreen");
#endif
            QVariantMap m = {{ "camModel", QVariant::fromValue(getItemModel(i)) }};
            appDispatcher->dispatch("openCameraSingleView", m);
        }
    }
}

QObject *CameraModel::getItemModel(int idx)
{
    CameraItem *obj = dynamic_cast<CameraItem *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

CameraItem::CameraItem(CalaosConnection *con, const ImageCachePtr &cache, CameraFrameDecoder *frameDecoder):
    QStandardItem(),
    connection(con),
    imageCache(cache),
    decoder(frameDecoder),
    watchdogMs(CameraPollWatchdogMs)
{
    update_hasPTZ(false);

    //One single timer per camera. It is the only object able to re-arm a
    //picture request, which structurally caps the number of polling chains of
    //this camera to one: restarting a timer never creates a second timeout.
    pollTimer = new QTimer(this);
    pollTimer->setSingleShot(true);
    connect(pollTimer, &QTimer::timeout, this, &CameraItem::sendPictureRequest);

    connect(connection, &CalaosConnection::cameraPictureDownloaded,
            this, &CameraItem::cameraPictureDownloaded);
    connect(connection, &CalaosConnection::cameraPictureFailed,
            this, &CameraItem::cameraPictureFailed);
    connect(connection, &CalaosConnection::cameraPictureProgress,
            this, &CameraItem::cameraPictureProgress);
    connect(decoder, &CameraFrameDecoder::decoded,
            this, &CameraItem::frameDecoded, Qt::QueuedConnection);

    connect(this, &CameraItem::cameraVisibleChanged, this, [this](bool visible)
    {
        if (visible)
            startCamera();
        else
            stopCamera();
    });
}

void CameraItem::load(QVariantMap &d, int countId)
{
    QMap<QString, QVariant>::const_iterator i = d.constBegin();
    while (i != d.constEnd())
    {
        cameraData[i.key()] = i.value();
        ++i;
    }

    update_cameraId(cameraData[JsonKeys::Id].toString());
    update_hasPTZ(cameraData[JsonKeys::Ptz].toString() == "true");

    if (cameraData.contains(JsonKeys::UrlLowres))
    {
        update_cameraId(QString::number(countId));
        isV1 = true; //when url_lowres is present, assume we are talking to a V1 calaos-os
        update_v1Url(cameraData[JsonKeys::UrlLowres].toString());
    }
    update_name(cameraData[JsonKeys::Name].toString());
    //Publish the placeholder before advertising the url, so that the provider
    //thread always finds something in the cache for this camera
    publishImage(QImage(":/img/camera_nocam.png"));
    update_url_single(QString("image://camera/%1/%2")
                      .arg(get_cameraId())
                      .arg(QRandomGenerator::global()->generate()));

    qDebug() << "New camera loaded: " << get_name();
}

void CameraItem::addFeed(CameraFeed *feed)
{
    if (!feeds.contains(feed))
        feeds.append(feed);
    refreshVisibility();
}

void CameraItem::removeFeed(CameraFeed *feed)
{
    feeds.removeAll(feed);
    refreshVisibility();
}

void CameraItem::setInBackground(bool background)
{
    inBackground = background;
    refreshVisibility();
}

void CameraItem::refreshVisibility()
{
    update_cameraVisible(!feeds.isEmpty() && !inBackground);
}

QSize CameraItem::decodeSize() const
{
    QSize largest;
    for (const CameraFeed *feed: feeds)
    {
        if (feed->frameSize().isEmpty())
            return QSize();
        largest = largest.expandedTo(feed->frameSize());
    }
    return largest;
}

int CameraItem::pollIntervalMs() const
{
    int shortest = std::numeric_limits<int>::max();
    for (const CameraFeed *feed: feeds)
        shortest = qMin(shortest, feed->interval());
    return shortest;
}

void CameraItem::publishImage(const QImage &image)
{
    if (imageCache)
        imageCache->setImage(get_cameraId(), image);
}

void CameraItem::cameraPictureDownloaded(const QString &camid, const QByteArray &data)
{
    //pollInFlight: the reply of a previous item of the same id, after a reload
    if (camid != get_cameraId() || !pollInFlight)
        return;

    pendingDecode = decoder->submit(data, decodeSize());
}

void CameraItem::frameDecoded(quint64 ticket, const QImage &image)
{
    if (ticket != pendingDecode)
        return;

    pendingDecode = 0;

    //A broken frame keeps the last good picture on screen
    if (image.isNull())
    {
        qDebug() << "Camera frame could not be decoded" << get_cameraId();
    }
    else
    {
        publishImage(image);
        update_url_single(QString("image://camera/%1/%2")
                          .arg(get_cameraId())
                          .arg(QRandomGenerator::global()->generate()));
    }

    pictureDone();
}

void CameraItem::cameraPictureFailed(const QString &camid)
{
    if (camid != get_cameraId() || !pollInFlight)
        return;

    qDebug() << "Camera picture download failed " << camid;

    pictureDone();
}

void CameraItem::cameraPictureProgress(const QString &camid)
{
    //pollInFlight: outside a request the timer holds the poll interval
    if (camid == get_cameraId() && pollInFlight)
        pollTimer->start(watchdogMs);
}

void CameraItem::pictureDone()
{
    pollInFlight = false;

    //Hiding the camera cancels the request, so it is still visible here.
    //Restarting the single member timer replaces the watchdog armed by
    //sendPictureRequest(), it never adds a second pending timeout.
    pollTimer->start(pollIntervalMs());
}

void CameraItem::cameraMoveUp()
{
    qDebug() << "Move camera up " << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move up",
                "output",
                "set_state");
    }
}

void CameraItem::cameraMoveDown()
{
    qDebug() << "Move camera down " << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move down",
                "output",
                "set_state");
    }
}

void CameraItem::cameraMoveLeft()
{
    qDebug() << "Move camera left " << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move left",
                "output",
                "set_state");
    }
}

void CameraItem::cameraMoveRight()
{
    qDebug() << "Move camera right " << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move right",
                "output",
                "set_state");
    }
}

void CameraItem::cameraMoveStop()
{
    qDebug() << "Stop moving Camera" << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move stop",
                "output",
                "set_state");
    }
}

void CameraItem::cameraZoomIn()
{
    qDebug() << "Zoom in camera" << get_cameraId();
    if (get_cameraVisible())
    {
            connection->sendCommand(get_cameraId(),
                    "move zoomin",
                    "output",
                    "set_state");
    }
}

void CameraItem::cameraZoomOut()
{
    qDebug() << "Zoom out camera" << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move zoomout",
                "output",
                "set_state");
    }
}

void CameraItem::cameraZoomStop()
{
    qDebug() << "Stop zooming camera" << get_cameraId();
    if (get_cameraVisible())
    {
        connection->sendCommand(get_cameraId(),
                "move zoomstop",
                "output",
                "set_state");
    }
}

void CameraItem::sendPictureRequest()
{
    //Arming the watchdog marks the chain as running, so that any concurrent
    //startCamera() becomes a no-op, and guarantees the chain survives a lost
    //reply. When it is the watchdog calling, a frame stuck in the decoder is
    //abandoned and the connection replaces the lost request.
    pollInFlight = true;
    pendingDecode = 0;
    pollTimer->start(watchdogMs);
    sinceLastRequest.start();
    connection->getCameraPicture(get_cameraId(), get_v1Url());
}

void CameraItem::startCamera()
{
    if (!get_cameraVisible())
        return;

    //A chain is already running: the next request is already armed
    if (pollTimer->isActive())
        return;

    //Entering the view is not a reason to skip the poll interval. Without this,
    //navigating in and out faster than the interval costs one request per
    //entry: measured at 8.05 req/s per camera against 4.94 in steady state on
    //60 ms toggles. Bounded, but it is still a rate increase.
    if (sinceLastRequest.isValid())
    {
        const qint64 waited = sinceLastRequest.elapsed();
        if (waited < pollIntervalMs())
        {
            pollTimer->start(int(pollIntervalMs() - waited));
            return;
        }
    }

    qDebug() << "Start camera " << get_cameraId();
    sendPictureRequest();
}

void CameraItem::stopCamera()
{
    pollTimer->stop();

    //Nobody is left for this frame: drop it wherever it is, on the network or
    //in the decoder
    pendingDecode = 0;
    if (pollInFlight)
    {
        pollInFlight = false;
        connection->cancelCameraPicture(get_cameraId());
    }
}
