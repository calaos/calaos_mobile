#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QObject>
#include <QElapsedTimer>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"
#include "ModelImageProvider.h"
#include <QList>
#include <QPointer>
#include <QQuickImageProvider>
#include <QSize>
#include <QTimer>

class CameraFeed;
class CameraFrameDecoder;
class CameraItem;
class CameraModel;

//Thin specialization, all the logic lives in ModelImageProvider
class CameraImageProvider: public ModelImageProvider
{
public:
    explicit CameraImageProvider(const ImageCachePtr &cache):
        ModelImageProvider(cache)
    { }
};


//What one view asks of one camera. A camera is polled as long as an active feed
//holds it, so the requests follow what is on screen and stop with the view,
//however it was left.
class CameraFeed: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QObject *camera READ camera WRITE setCamera NOTIFY cameraChanged)
    //Off by default: QML sets properties in any order, so a feed that starts
    //active would send a request before its binding switches it off.
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    //Pixels actually displayed. Empty means full resolution.
    Q_PROPERTY(QSize frameSize READ frameSize WRITE setFrameSize NOTIFY frameSizeChanged)
    //Delay between two frames, never below the default.
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)

public:
    explicit CameraFeed(QObject *parent = nullptr);
    ~CameraFeed() override;

    QObject *camera() const;
    void setCamera(QObject *camera);
    bool active() const { return feedActive; }
    void setActive(bool active);
    QSize frameSize() const { return size; }
    void setFrameSize(const QSize &frameSize);
    int interval() const { return intervalMs; }
    void setInterval(int interval);

signals:
    void cameraChanged();
    void activeChanged();
    void frameSizeChanged();
    void intervalChanged();

private:
    //Cleared by itself when a reload of the model deletes the camera
    QPointer<CameraItem> item;
    bool feedActive = false;
    QSize size;
    int intervalMs;
};

class CameraModel: public QStandardItemModel
{
    Q_OBJECT

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
    void applicationStateChanged(Qt::ApplicationState state);

private:

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    //Owned by the QML engine
    CameraImageProvider *imgProvider = nullptr;
    //Shared with the provider and with every CameraItem
    ImageCachePtr imageCache;
    //Child of the model, so it outlives the items the model deletes
    CameraFrameDecoder *decoder = nullptr;
    bool inBackground = false;
};

class CameraItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, name, CameraModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, url_single, CameraModel::RoleUrl)
    QML_READONLY_PROPERTY_MODEL(QString, cameraId, CameraModel::RoleId)
    QML_READONLY_PROPERTY_MODEL(bool, hasPTZ, CameraModel::RolePTZ)
    //True while an active feed holds this camera and the application is in the
    //foreground
    QML_READONLY_PROPERTY(bool, cameraVisible)
    QML_READONLY_PROPERTY(QString, v1Url)

public:
    CameraItem(CalaosConnection *con, const ImageCachePtr &cache, CameraFrameDecoder *frameDecoder);

    void load(QVariantMap &d, int countId);
    void addFeed(CameraFeed *feed);
    void removeFeed(CameraFeed *feed);
    void setInBackground(bool background);

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
    void frameDecoded(quint64 ticket, const QImage &image);

private:
    //Sends one picture request and arms the watchdog
    void sendPictureRequest();
    //End of a request, in every outcome: arms the next one
    void pictureDone();
    //Publishes a copy of the image into the cache read by the provider thread
    void publishImage(const QImage &image);
    void refreshVisibility();
    //Largest size asked by the feeds, empty as soon as one wants it all
    QSize decodeSize() const;
    //Shortest delay asked by the feeds
    int pollIntervalMs() const;

    QVariantMap cameraData;
    CalaosConnection *connection;
    ImageCachePtr imageCache;
    CameraFrameDecoder *decoder;

    //Not owning: a feed removes itself when destroyed
    QList<CameraFeed *> feeds;
    bool inBackground = false;

    //The one and only timer able to re-arm a picture request for this camera.
    //As long as it is active, a polling chain is running and startCamera() will
    //not start another one.
    QTimer *pollTimer = nullptr;
    //From the request until its frame is decoded: the next request waits for
    //that, so a slow network or decoder drops frames instead of queueing them.
    bool pollInFlight = false;
    //Ticket of the frame being decoded, 0 when none is wanted
    quint64 pendingDecode = 0;
    //When the last request went out, so that re-entering the camera view does
    //not fire one immediately and let fast navigation raise the request rate.
    QElapsedTimer sinceLastRequest;

    bool isV1 = false;
};

#endif // CAMERAMODEL_H
