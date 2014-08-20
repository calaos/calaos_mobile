#include "AudioModel.h"

AudioModel::AudioModel(QQmlApplicationEngine *eng, CalaosConnection *con, QObject *parent):
    QStandardItemModel(parent),
    engine(eng),
    connection(con)
{
    QHash<int, QByteArray> roles;
    roles[RoleId] = "audioId";
    roles[RoleName] = "audioName";
    roles[RolePicSrc] = "audioCoverSource";
    roles[RoleTitle] = "audioTitle";
    roles[RoleVolume] = "audioVolume";
    roles[RoleAlbum] = "audioAlbum";
    roles[RoleArtist] = "audioArtist";
    roles[RoleStatus] = "audioStatus";
    setItemRoleNames(roles);
}

void AudioModel::load(QVariantMap &homeData)
{
    clear();

    if (!homeData.contains("audio"))
    {
        qDebug() << "no audio entry";
        return;
    }

    QVariantList players = homeData["audio"].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        AudioPlayer *p = new AudioPlayer(connection);
        p->load(r);
        appendRow(p);
    }
}

QObject *AudioModel::getItemModel(int idx)
{
    AudioPlayer *obj = dynamic_cast<AudioPlayer *>(item(idx));
    if (obj) engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);
    return obj;
}

AudioPlayer::AudioPlayer(CalaosConnection *con):
    QStandardItem(),
    connection(con)
{
}

void AudioPlayer::load(QVariantMap &d)
{
    QMap<QString, QVariant>::const_iterator i = d.constBegin();
    while (i != d.constEnd())
    {
        playerData[i.key()] = i.value();
        ++i;
    }

    update_status(Common::audioStatusFromString(playerData["status"].toString()));
    update_id(playerData["id"].toString());
    update_cover(playerData["cover_url"].toString());
    update_name(playerData["name"].toString());
    update_volume(playerData["volume"].toDouble());

    qDebug() << "New player loaded: " << get_name();

    QVariantMap currentTrack = playerData["current_track"].toMap();
    update_title(currentTrack["title"].toString());
    update_album(currentTrack["album"].toString());
    update_artist(currentTrack["artist"].toString());

    connect(connection, SIGNAL(eventAudioChange(QString)),
            this, SLOT(audioChanged(QString)));
    connect(connection, SIGNAL(eventAudioStatusChange(QString,QString)),
            this, SLOT(audioStatusChanged(QString,QString)));
    connect(connection, SIGNAL(eventAudioVolumeChange(QString,double)),
            this, SLOT(audioVolumeChanged(QString,double)));

    if (!loaded)
    {
        loaded = true;
        audioChanged(get_id());
    }
}

void AudioPlayer::sendNext()
{
    connection->sendCommand(playerData["id"].toString(),
            "next",
            "audio",
            "set_state");
}

void AudioPlayer::sendPause()
{
    connection->sendCommand(playerData["id"].toString(),
            "pause",
            "audio",
            "set_state");
}

void AudioPlayer::sendPlay()
{
    connection->sendCommand(playerData["id"].toString(),
            "play",
            "audio",
            "set_state");
    qDebug() << "play";
}

void AudioPlayer::sendPrevious()
{
    connection->sendCommand(playerData["id"].toString(),
            "previous",
            "audio",
            "set_state");
}

void AudioPlayer::sendStop()
{
    connection->sendCommand(playerData["id"].toString(),
            "stop",
            "audio",
            "set_state");
}

void AudioPlayer::audioChanged(QString playerid)
{
    if (playerid != playerData["id"].toString()) return;

    connection->queryState(QStringList(),
                           QStringList(),
                           QStringList() << playerid,
                           this,
                           "audioStateChanged");
}

void AudioPlayer::audioStateChanged(const QVariantMap &data)
{
    QVariantList players = data["audio_players"].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        if (r["player_id"].toString() == playerData["id"].toString())
        {
            load(r);
            break;
        }
    }
}

void AudioPlayer::audioStatusChanged(QString playerid, QString status)
{
    if (playerid != playerData["id"].toString()) return;

    playerData["volume"] = status;
    update_status(Common::audioStatusFromString(status));
}

void AudioPlayer::audioVolumeChanged(QString playerid, double volume)
{
    if (playerid != playerData["id"].toString()) return;

    playerData["volume"] = QString("%1").arg(volume);
    update_volume(volume);
}
