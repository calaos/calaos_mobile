#include "CameraModel.h"

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
    setItemRoleNames(roles);

    //add a special image provider for single pictures of cameras
    engine->addImageProvider(QLatin1String("camera"), this);
}

void CameraModel::load(QVariantMap &homeData)
{
    clear();

    if (!homeData.contains("cameras"))
    {
        qDebug() << "no camera entry";
        return;
    }

    QVariantList cameras = homeData["cameras"].toList();
    QVariantList::iterator it = cameras.begin();
    for (;it != cameras.end();it++)
    {
        QVariantMap r = it->toMap();
        CameraItem *p = new CameraItem(connection);
        p->load(r);
        appendRow(p);
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
}

void CameraItem::load(QVariantMap &d)
{
    QMap<QString, QVariant>::const_iterator i = d.constBegin();
    while (i != d.constEnd())
    {
        cameraData[i.key()] = i.value();
        ++i;
    }

    update_cameraId(cameraData["id"].toString());
    update_name(cameraData["name"].toString());
    update_url_single(QString("image://camera/%1/%2").arg(get_cameraId()).arg(qrand()));
    currentImage = QImage(":/img/camera_nocam.png");

    qDebug() << "New camera loaded: " << get_name();

    QTimer::singleShot(100, [=]()
    {
        connection->getCameraPicture(get_cameraId());
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

    QString id = sl.at(0);
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

void CameraItem::cameraPictureDownloaded(const QString &camid, const QString &pic, const QString &encoding, const QString &contenttype)
{
    Q_UNUSED(contenttype);
    if (camid != get_cameraId())
        return;

    if (encoding != "base64")
        return;

    currentImage = QImage::fromData(QByteArray::fromBase64(pic.toLatin1()));

    update_url_single(QString("image://camera/%1/%2").arg(get_cameraId()).arg(qrand()));
}
