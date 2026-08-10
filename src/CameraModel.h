#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"
#include "ModelImageProvider.h"
#include <QQuickImageProvider>
#include <QTimer>

class CameraModel;

//Thin specialization, all the logic lives in ModelImageProvider
class CameraImageProvider: public ModelImageProvider
{
public:
    explicit CameraImageProvider(const ImageCachePtr &cache):
        ModelImageProvider(cache)
    { }
};


class CameraModel: public QStandardItemModel
{
    Q_OBJECT

    QML_WRITABLE_PROPERTY(bool, cameraVisible)

public:
    CameraModel(QQmlApplicationEngine *engine, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleName = Qt::UserRole + 1,
        RoleId,
        RoleUrl,
        RolePTZ,
    };

    void load(const QVariantMap &homeData);

    Q_INVOKABLE QObject *getItemModel(int idx);
    Q_INVOKABLE int cameraCount() { return rowCount(); }

private slots:
    void eventTouchscreenCamera(QString cameraId);


private:

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    //Owned by the QML engine
    CameraImageProvider *imgProvider = nullptr;
    //Shared with the provider and with every CameraItem
    ImageCachePtr imageCache;
};

class CameraItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, name, CameraModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, url_single, CameraModel::RoleUrl)
    QML_READONLY_PROPERTY_MODEL(QString, cameraId, CameraModel::RoleId)
    QML_READONLY_PROPERTY_MODEL(bool, hasPTZ, CameraModel::RolePTZ)
    QML_WRITABLE_PROPERTY(bool, cameraVisible)
    QML_READONLY_PROPERTY(QString, v1Url)

public:
    CameraItem(CalaosConnection *con, const ImageCachePtr &cache);

    void load(QVariantMap &d, int countId);
    Q_INVOKABLE void cameraMoveUp();
    Q_INVOKABLE void cameraMoveDown();
    Q_INVOKABLE void cameraMoveRight();
    Q_INVOKABLE void cameraMoveLeft();
    Q_INVOKABLE void cameraMoveStop();
    Q_INVOKABLE void cameraZoomIn();
    Q_INVOKABLE void cameraZoomOut();
    Q_INVOKABLE void cameraZoomStop();

    //Starts the polling chain of this camera. Idempotent: calling it while a
    //chain is already running is a no-op.
    void startCamera();
    //Stops the polling chain of this camera.
    void stopCamera();

    bool isV1Camera() { return isV1; }

signals:
    void newFrameReceived();

private slots:
    void cameraPictureDownloaded(const QString &camid, const QByteArray &data);
    void cameraPictureFailed(const QString &camid);

private:
    //Sends one picture request and arms the watchdog
    void sendPictureRequest();
    //Common tail of both reply handlers: closes the request and re-arms the
    //next one when the camera is still visible
    void pictureReplyDone();
    //Publishes a copy of the image into the cache read by the provider thread
    void publishImage(const QImage &image);

    QVariantMap cameraData;
    CalaosConnection *connection;
    ImageCachePtr imageCache;

    //The one and only timer able to re-arm a picture request for this camera.
    //As long as it is active (or pollInFlight is set), a polling chain is
    //running and startCamera() will not start another one.
    QTimer *pollTimer = nullptr;
    bool pollInFlight = false;

    bool isV1 = false;
};

#endif // CAMERAMODEL_H
