#ifndef AUDIOMODEL_H
#define AUDIOMODEL_H

#include <QObject>
#include <QStandardItemModel>
#include "qqmlhelpers.h"
#include <QQmlApplicationEngine>
#include "CalaosConnection.h"
#include "Common.h"

class AudioModel: public QStandardItemModel
{
    Q_OBJECT
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
        RoleStatus
    };

    void load(QVariantMap &homeData);

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
    QML_READONLY_PROPERTY_MODEL(double, volume, AudioModel::RoleVolume)
    QML_READONLY_PROPERTY_MODEL(QString, cover, AudioModel::RolePicSrc)

public:
    AudioPlayer(CalaosConnection *con);

    void load(QVariantMap &d);

    Q_INVOKABLE void sendPlay();
    Q_INVOKABLE void sendPause();
    Q_INVOKABLE void sendStop();
    Q_INVOKABLE void sendNext();
    Q_INVOKABLE void sendPrevious();

private slots:
    void audioChange(QString playerid);
    void audioStatusChange(QString playerid, QString status);
    void audioVolumeChange(QString playerid, double volume);

    void audioStateChange(QVariantMap &data);

private:
    QVariantMap playerData;
    CalaosConnection *connection;

};

#endif // AUDIOMODEL_H
