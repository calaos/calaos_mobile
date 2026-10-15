#ifndef AUDIOMODEL_H
#define AUDIOMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"
#include "ModelImageProvider.h"
#include <QQuickImageProvider>
#include <QTimer>

class AudioModel;

//Thin specialization, all the logic lives in ModelImageProvider
class AudioImageProvider: public ModelImageProvider
{
public:
    explicit AudioImageProvider(const ImageCachePtr &cache):
        ModelImageProvider(cache)
    { }
};

class AudioModel: public QStandardItemModel
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

private:

    QQmlApplicationEngine *engine = nullptr;
    CalaosConnection *connection = nullptr;
    //Owned by the QML engine
    AudioImageProvider *imgProvider = nullptr;
    //Shared with the provider and with every AudioPlayer
    ImageCachePtr imageCache;
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
    AudioPlayer(CalaosConnection *con, const ImageCachePtr &cache);

    void load(QVariantMap &d);

    //Both are idempotent, the poll timer is created once and reused
    void startPolling();
    void stopPolling();

    Q_INVOKABLE void sendPlay();
    Q_INVOKABLE void sendPause();
    Q_INVOKABLE void sendStop();
    Q_INVOKABLE void sendNext();
    Q_INVOKABLE void sendPrevious();
    Q_INVOKABLE void sendVolume(int vol);

public slots:
    void audioChanged(QString playerid);
    void audioStatusChanged(QString playerid, QString status);
    void audioVolumeChanged(QString playerid, double volume);
    void audioStateChanged(QString playerid, const QVariantMap &data);
    void audioCoverDownloaded(QString playerid, const QByteArray &data);

private:
    QVariantMap playerData;
    CalaosConnection *connection;
    ImageCachePtr imageCache;
    bool loaded;
    QTimer *pollTimer = nullptr;

    void updatePlayerState(const QVariantMap &data);
};

#endif // AUDIOMODEL_H
