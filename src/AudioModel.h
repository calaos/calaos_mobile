#ifndef AUDIOMODEL_H
#define AUDIOMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"
#include <QQuickImageProvider>

class AudioModel: public QStandardItemModel, public QQuickImageProvider
{
    Q_OBJECT

    QML_WRITABLE_PROPERTY(bool, playersVisible)

public:
    AudioModel(QQmlApplicationEngine *engine, CalaosConnection *con, QObject *parent = 0);

    enum
    {
        RoleName = Qt::UserRole + 1,
        RoleId,
        RoleTitle,
        RoleVolume,
        RolePicSrc,
        RoleAlbum,
        RoleArtist,
        RoleStatus,
        RoleDuration,
        RoleElapsed,
        RoleGenre,
        RoleYear,
    };

    void load(const QVariantMap &homeData);

    Q_INVOKABLE QObject *getItemModel(int idx);
    Q_INVOKABLE int audioCount() { return rowCount(); }

    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize);
    virtual QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize);

private:

    QQmlApplicationEngine *engine;
    CalaosConnection *connection;
};

class AudioPlayer: public QObject, public QStandardItem
{
    Q_OBJECT

    QML_READONLY_PROPERTY_MODEL(Common::AudioStatusType, status, AudioModel::RoleStatus)
    QML_READONLY_PROPERTY_MODEL(QString, title, AudioModel::RoleTitle)
    QML_READONLY_PROPERTY_MODEL(QString, album, AudioModel::RoleAlbum)
    QML_READONLY_PROPERTY_MODEL(QString, artist, AudioModel::RoleArtist)
    QML_READONLY_PROPERTY_MODEL(QString, name, AudioModel::RoleName)
    QML_READONLY_PROPERTY_MODEL(QString, id, AudioModel::RoleId)
    QML_READONLY_PROPERTY_MODEL(QString, genre, AudioModel::RoleGenre)
    QML_READONLY_PROPERTY_MODEL(QString, year, AudioModel::RoleYear)
    QML_READONLY_PROPERTY_MODEL(double, duration, AudioModel::RoleDuration)
    QML_READONLY_PROPERTY_MODEL(double, elapsed, AudioModel::RoleElapsed)
    QML_READONLY_PROPERTY_MODEL(double, volume, AudioModel::RoleVolume)
    QML_READONLY_PROPERTY_MODEL(QString, cover, AudioModel::RolePicSrc)

    QML_WRITABLE_PROPERTY(bool, playerVisible)

public:
    AudioPlayer(CalaosConnection *con);

    void load(QVariantMap &d);

    void startPolling();
    void stopPolling();

    Q_INVOKABLE void sendPlay();
    Q_INVOKABLE void sendPause();
    Q_INVOKABLE void sendStop();
    Q_INVOKABLE void sendNext();
    Q_INVOKABLE void sendPrevious();
    Q_INVOKABLE void sendVolume(int vol);

    void getCurrentCoverImage(QImage &image);

public slots:
    void audioChanged(QString playerid);
    void audioStatusChanged(QString playerid, QString status);
    void audioVolumeChanged(QString playerid, double volume);
    void audioStateChanged(QString playerid, const QVariantMap &data);
    void audioCoverDownloaded(QString playerid, const QByteArray &data);

private:
    QVariantMap playerData;
    CalaosConnection *connection;
    bool loaded;
    QTimer *pollTimer = nullptr;
    QImage currentCoverImage;

    void updatePlayerState(const QVariantMap &data);
};

#endif // AUDIOMODEL_H
