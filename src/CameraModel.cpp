#include "CameraModel.h"
#include "ScreenManager.h"
#include <qfappdispatcher.h>

CameraModel::CameraModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    QQuickImageProvider(QQuickImageProvider::Image),
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
    engine->addImageProvider(QLatin1String("camera"), this);

    connect(this, &CameraModel::cameraVisibleChanged, this, [=](bool visible)
    {
        for (int i = 0;i < rowCount();i++)
        {
            CameraItem *obj = dynamic_cast<CameraItem *>(item(i));
            obj->set_cameraVisible(visible);
            if (visible)
                obj->startCamera();
        }
    });

    connect(connection, &CalaosConnection::eventTouchscreenCamera,
            this, &CameraModel::eventTouchscreenCamera);
}

void CameraModel::load(const QVariantMap &homeData)
{
    clear();

    if (!homeData.contains("cameras"))
    {
        qDebug() << "no camera entry";
        return;
    }

    QVariantList cameras = homeData["cameras"].toList();
    QVariantList::iterator it = cameras.begin();
    for (int i = 0;it != cameras.end();it++, i++)
    {
        QVariantMap r = it->toMap();
        CameraItem *p = new CameraItem(connection);
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
#ifdef CALAOS_DESKTOP
            ScreenManager::Instance().wakeupScreen();

            //Also tell ScreenSuspend.qml to wake up
            QFAppDispatcher *appDispatcher = QFAppDispatcher::instance(engine);
            appDispatcher->dispatch("wakeupScreen");
#endif
            emit actionViewCamera(getItemModel(i));
        }
    }
}

QObject *CameraModel::getItemModel(int idx)
{
    CameraItem *obj = dynamic_cast<CameraItem *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

CameraItem::CameraItem(CalaosConnection *con):
    QStandardItem(),
    connection(con)
{
    set_cameraVisible(false);
    update_hasPTZ(false);
    connect(connection, SIGNAL(cameraPictureDownloaded(QString,QByteArray)),
            this, SLOT(cameraPictureDownloaded(QString,QByteArray)));
    connect(connection, SIGNAL(cameraPictureFailed(QString)),
            this, SLOT(cameraPictureFailed(QString)));

    connect(this, &CameraItem::cameraVisibleChanged, [=](bool visible)
    {
        if (visible)
            startCamera();
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

    update_cameraId(cameraData["id"].toString());
    update_hasPTZ(cameraData["ptz"].toString() == "true");

    if (cameraData.contains("url_lowres"))
    {
        update_cameraId(QString::number(countId));
        isV1 = true; //when url_lowres is present, assume we are talking to a V1 calaos-os
        update_v1Url(cameraData["url_lowres"].toString());
    }
    update_name(cameraData["name"].toString());
    update_url_single(QString("image://camera/%1/%2").arg(get_cameraId()).arg(qrand()));
    currentImage = QImage(":/img/camera_nocam.png");

    qDebug() << "New camera loaded: " << get_name();

    QTimer::singleShot(100, this, [=]()
    {
        connection->getCameraPicture(get_cameraId(), get_v1Url());
    });
}

void CameraItem::getPictureImage(QImage &image)
{
    image = currentImage;
}

QImage CameraModel::requestImage(const QString &qid, QSize *size, const QSize &requestedSize)
{
    QImage retimg;

    QStringList sl = qid.split('/');
    if (sl.empty()) return retimg;

    const QString& id = sl.at(0);
    CameraItem *cam = nullptr;

    if (id.toInt() < 0)
        return retimg;

    for (int i = 0;i < rowCount();i++)
    {
        CameraItem *c = dynamic_cast<CameraItem *>(item(i));
        if (c->get_cameraId() == id)
        {
            cam = c;
            break;
        }
    }
    if (!cam)
        return retimg;

    cam->getPictureImage(retimg);

    *size = retimg.size();
    if (requestedSize.isValid())
        return retimg.scaled(requestedSize, Qt::KeepAspectRatio);

    return retimg;
}

QPixmap CameraModel::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    return QPixmap::fromImage(requestImage(id, size, requestedSize));
}

void CameraItem::cameraPictureDownloaded(const QString &camid, const QByteArray &data)
{
    if (camid != get_cameraId())
        return;

    currentImage = QImage::fromData(data);

    update_url_single(QString("image://camera/%1/%2").arg(get_cameraId()).arg(qrand()));

    if (get_cameraVisible())
    {
        QTimer::singleShot(200, this, [=]()
        {
            connection->getCameraPicture(get_cameraId(), get_v1Url());
        });
    }
}

void CameraItem::cameraPictureFailed(const QString &camid)
{
    if (camid != get_cameraId())
        return;

    qDebug() << "Camera picture download failed " << camid;

    if (get_cameraVisible())
    {
        QTimer::singleShot(200, this, [=]()
        {
            connection->getCameraPicture(get_cameraId(), get_v1Url());
        });
    }
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

void CameraItem::startCamera()
{
    QTimer::singleShot(0, this, [=]()
    {
        qDebug() << "Start camera " << get_cameraId();
        connection->getCameraPicture(get_cameraId(), get_v1Url());
    });
}
