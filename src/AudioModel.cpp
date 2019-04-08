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

    set_playersVisible(false);

    connect(this, &AudioModel::playersVisibleChanged, this, [=](bool visible)
    {
        for (int i = 0;i < rowCount();i++)
        {
            auto obj = dynamic_cast<AudioPlayer *>(item(i));
            obj->set_playerVisible(visible);
            if (visible)
                obj->startPolling();
            else
                obj->stopPolling();
        }
    });
}

void AudioModel::load(const QVariantMap &homeData)
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
    loaded = false;
}

void AudioPlayer::updatePlayerState(const QVariantMap &d)
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
    update_elapsed(playerData["time_elapsed"].toDouble());

    QVariantMap currentTrack = playerData["current_track"].toMap();
    update_title(currentTrack["title"].toString());
    update_album(currentTrack["album"].toString());
    update_artist(currentTrack["artist"].toString());
    update_genre(currentTrack["genre"].toString());
    update_year(currentTrack["year"].toString());
    update_duration(currentTrack["duration"].toDouble());
}

void AudioPlayer::load(QVariantMap &d)
{
    updatePlayerState(d);

    qDebug() << "New player loaded: " << get_name();

    connect(connection, &CalaosConnection::eventAudioChange, this, &AudioPlayer::audioChanged);
    connect(connection, &CalaosConnection::eventAudioStateChange, this, &AudioPlayer::audioStateChanged);
    connect(connection, &CalaosConnection::eventAudioStatusChange, this, &AudioPlayer::audioStatusChanged);
    connect(connection, &CalaosConnection::eventAudioVolumeChange, this, &AudioPlayer::audioVolumeChanged);

    if (!loaded)
    {
        loaded = true;

        //query initial state
        connection->queryState(QStringList(),
                               QStringList(),
                               QStringList() << get_id());
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

void AudioPlayer::sendVolume(int vol)
{
    connection->sendCommand(playerData["id"].toString(),
            QStringLiteral("volume set %1").arg(vol),
            "audio",
            "set_state");
}

void AudioPlayer::audioChanged(QString playerid)
{
    if (playerid != get_id()) return;

    connection->queryState(QStringList(),
                           QStringList(),
                           QStringList() << playerid);
}

void AudioPlayer::audioStateChanged(QString playerid, const QVariantMap &data)
{
    if (!playerid.isEmpty())
    {
        if (playerid != get_id()) return;
        updatePlayerState(data);
        return;
    }

    //this is for old v1/v2
    QVariantList players = data["audio_players"].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        if (r["player_id"].toString() == playerData["id"].toString())
        {
            updatePlayerState(data);
            break;
        }
    }
}

void AudioPlayer::audioStatusChanged(QString playerid, QString status)
{
    if (playerid != get_id()) return;

    playerData["volume"] = status;
    update_status(Common::audioStatusFromString(status));
}

void AudioPlayer::audioVolumeChanged(QString playerid, double volume)
{
    if (playerid != get_id()) return;

    playerData["volume"] = QString("%1").arg(volume);
    update_volume(volume);
}

void AudioPlayer::startPolling()
{
    if (pollTimer) delete pollTimer;
    pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [=]()
    {
        connection->queryState(QStringList(),
                               QStringList(),
                               QStringList() << get_id());
    });
    pollTimer->start(1000);
}

void AudioPlayer::stopPolling()
{
    delete pollTimer;
    pollTimer = nullptr;
}
