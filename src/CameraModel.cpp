#include "CameraModel.h"
#include "ScreenManager.h"
#include "JsonKeys.h"
#include <qfappdispatcher.h>

//Delay between two picture requests of a visible camera
static const int CameraPollIntervalMs = 200;
//Safety net: if a reply is ever lost, retry after this delay instead of
//freezing the camera forever
static const int CameraPollWatchdogMs = 5000;
//Delay before the very first picture request of a freshly loaded camera
static const int CameraFirstPictureDelayMs = 100;

CameraModel::CameraModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleId] = "cameraId";
    roles[RoleName] = "name";
    roles[RoleUrl] = "url_single";
    roles[RolePTZ] = "hasPTZ";
    setItemRoleNames(roles);

    set_cameraVisible(false);

    //add a special image provider for single pictures of cameras
    //The cache is shared with the items, the provider itself is owned by the engine
    imageCache = ImageCachePtr::create();
    imgProvider = new CameraImageProvider(imageCache);
    engine->addImageProvider(QLatin1String("camera"), imgProvider);

    connect(this, &CameraModel::cameraVisibleChanged, this, [this](bool visible)
    {
        for (int i = 0;i < rowCount();i++)
        {
            CameraItem *obj = dynamic_cast<CameraItem *>(item(i));
            if (!obj)
            {
                qWarning() << "CameraModel: row" << i << "is not a CameraItem, skipping";
                continue;
            }
            //set_cameraVisible already starts or stops the chain through the
            //item's own cameraVisibleChanged handler, calling it again here
            //was one of the ways chains used to pile up.
            obj->set_cameraVisible(visible);
        }
    });

    connect(connection, &CalaosConnection::eventTouchscreenCamera,
            this, &CameraModel::eventTouchscreenCamera);
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
        CameraItem *p = new CameraItem(connection, imageCache);
        p->load(r, i);
        appendRow(p);
        //Fresh items default to hidden, but the model level property survives
        //the reload. A reconnection while the camera view is open would
        //otherwise leave every item hidden with the model still saying
        //visible, and nothing would poll again until the user left the media
        //section and came back.
        if (get_cameraVisible())
            p->set_cameraVisible(true);
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

CameraItem::CameraItem(CalaosConnection *con, const ImageCachePtr &cache):
    QStandardItem(),
    connection(con),
    imageCache(cache)
{
    set_cameraVisible(false);
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

    //Fetch a first picture even though the camera is not visible yet. The reply
    //handler only re-arms the timer when the camera is visible, so this single
    //request does not turn into a polling chain.
    pollTimer->start(CameraFirstPictureDelayMs);
}

void CameraItem::publishImage(const QImage &image)
{
    if (imageCache)
        imageCache->setImage(get_cameraId(), image);
}

void CameraItem::cameraPictureDownloaded(const QString &camid, const QByteArray &data)
{
    if (camid != get_cameraId())
        return;

    publishImage(QImage::fromData(data));

    update_url_single(QString("image://camera/%1/%2")
                      .arg(get_cameraId())
                      .arg(QRandomGenerator::global()->generate()));

    pictureReplyDone();
}

void CameraItem::cameraPictureFailed(const QString &camid)
{
    if (camid != get_cameraId())
        return;

    qDebug() << "Camera picture download failed " << camid;

    pictureReplyDone();
}

void CameraItem::pictureReplyDone()
{
    pollInFlight = false;

    //Restarting the single member timer replaces the watchdog armed by
    //sendPictureRequest(), it never adds a second pending timeout.
    if (get_cameraVisible())
        pollTimer->start(CameraPollIntervalMs);
    else
        pollTimer->stop();
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
    //reply.
    pollInFlight = true;
    pollTimer->start(CameraPollWatchdogMs);
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

    if (pollInFlight)
    {
        //A request issued before the camera was hidden is still pending. Do not
        //send a second one, only re-arm the watchdog so that polling resumes
        //even if that reply never comes back.
        pollTimer->start(CameraPollWatchdogMs);
        return;
    }

    //Entering the view is not a reason to skip the poll interval. Without this,
    //navigating in and out faster than the interval costs one request per
    //entry: measured at 8.05 req/s per camera against 4.94 in steady state on
    //60 ms toggles. Bounded, but it is still a rate increase.
    if (sinceLastRequest.isValid())
    {
        const qint64 waited = sinceLastRequest.elapsed();
        if (waited < CameraPollIntervalMs)
        {
            pollTimer->start(int(CameraPollIntervalMs - waited));
            return;
        }
    }

    qDebug() << "Start camera " << get_cameraId();
    sendPictureRequest();
}

void CameraItem::stopCamera()
{
    //Stopping the timer kills the chain. pollInFlight is left untouched on
    //purpose: an already sent request cannot be cancelled, and its reply
    //handler will not re-arm anything while the camera is hidden.
    pollTimer->stop();
}
