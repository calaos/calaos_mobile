#ifndef CAMERAMODEL_H
#define CAMERAMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"
#include <QQuickImageProvider>

class CameraModel;

class CameraImageProvider: public QQuickImageProvider
{
public:
    CameraImageProvider(CameraModel *model);

    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

private:
    CameraModel *model = nullptr;
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
    };

    void load(const QVariantMap &homeData);

    Q_INVOKABLE QObject *getItemModel(int idx);
    Q_INVOKABLE int cameraCount() { return rowCount(); }

signals:
    void actionViewCamera(QObject *camModel);

private slots:
    void eventTouchscreenCamera(QString cameraId);

private:

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    CameraImageProvider *imgProvider = nullptr;
};

class CameraItem: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(QString, name, CameraModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, url_single, CameraModel::RoleUrl)
    QML_READONLY_PROPERTY_MODEL(QString, cameraId, CameraModel::RoleId)
    QML_WRITABLE_PROPERTY(bool, cameraVisible)
    QML_READONLY_PROPERTY(QString, v1Url)

public:
    CameraItem(CalaosConnection *con);

    void load(QVariantMap &d, int countId);

    void getPictureImage(QImage &image);

    void startCamera();

    bool isV1Camera() { return isV1; }

signals:
    void newFrameReceived();

private slots:
    void cameraPictureDownloaded(const QString &camid, const QByteArray &data);
    void cameraPictureFailed(const QString &camid);

private:
    QVariantMap cameraData;
    CalaosConnection *connection;
    QImage currentImage;

    bool isV1 = false;
};

#endif // CAMERAMODEL_H
