#include "AudioModel.h"
#include "JsonKeys.h"

//Delay between two state queries of a visible player
static const int AudioPollIntervalMs = 1000;

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

    //add a special image provider for album covers
    //The cache is shared with the items, the provider itself is owned by the engine
    imageCache = ImageCachePtr::create();
    imgProvider = new AudioImageProvider(imageCache);
    engine->addImageProvider(QLatin1String("audio_cover"), imgProvider);

    connect(this, &AudioModel::playersVisibleChanged, this, [this](bool visible)
    {
        for (int i = 0;i < rowCount();i++)
        {
            auto obj = dynamic_cast<AudioPlayer *>(item(i));
            if (!obj)
            {
                qWarning() << "AudioModel: row" << i << "is not an AudioPlayer, skipping";
                continue;
            }
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
    imageCache->clear();

    if (!homeData.contains(JsonKeys::Audio))
    {
        qDebug() << "no audio entry";
        return;
    }

    QVariantList players = homeData[JsonKeys::Audio].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        AudioPlayer *p = new AudioPlayer(connection, imageCache);
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

AudioPlayer::AudioPlayer(CalaosConnection *con, const ImageCachePtr &cache):
    QStandardItem(),
    connection(con),
    imageCache(cache)
{
    loaded = false;

    connect(connection, &CalaosConnection::audioCoverDownloaded, this, &AudioPlayer::audioCoverDownloaded);
}

void AudioPlayer::updatePlayerState(const QVariantMap &d)
{
    QMap<QString, QVariant>::const_iterator i = d.constBegin();
    while (i != d.constEnd())
    {
        playerData[i.key()] = i.value();
        ++i;
    }
    //refresh audio cover
    connection->getAudioCover(get_id());

    update_status(Common::audioStatusFromString(playerData[JsonKeys::Status].toString()));
    update_id(playerData[JsonKeys::Id].toString());
    update_name(playerData[JsonKeys::Name].toString());
    auto vol = Common::toDoubleSafe(playerData[JsonKeys::Volume], 0.0, "AudioPlayer.volume");
    if (get_volume() != vol)
        update_volume(vol);
    update_elapsed(Common::toDoubleSafe(playerData[JsonKeys::TimeElapsed], 0.0, "AudioPlayer.time_elapsed"));

    QVariantMap currentTrack = playerData[JsonKeys::CurrentTrack].toMap();
    update_title(currentTrack[JsonKeys::Title].toString());
    update_album(currentTrack[JsonKeys::Album].toString());
    update_artist(currentTrack[JsonKeys::Artist].toString());
    update_genre(currentTrack[JsonKeys::Genre].toString());
    update_year(currentTrack[JsonKeys::Year].toString());
    update_duration(Common::toDoubleSafe(currentTrack[JsonKeys::Duration], 0.0, "AudioPlayer.current_track.duration"));
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
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
            "next",
            "audio",
            "set_state");
}

void AudioPlayer::sendPause()
{
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
            "pause",
            "audio",
            "set_state");
}

void AudioPlayer::sendPlay()
{
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
            "play",
            "audio",
            "set_state");
}

void AudioPlayer::sendPrevious()
{
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
            "previous",
            "audio",
            "set_state");
}

void AudioPlayer::sendStop()
{
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
            "stop",
            "audio",
            "set_state");
}

void AudioPlayer::sendVolume(int vol)
{
    connection->sendCommand(playerData[JsonKeys::Id].toString(),
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
    QVariantList players = data[JsonKeys::AudioPlayers].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        if (r[JsonKeys::PlayerId].toString() == playerData[JsonKeys::Id].toString())
        {
            updatePlayerState(data);
            break;
        }
    }
}

void AudioPlayer::audioStatusChanged(QString playerid, QString status)
{
    if (playerid != get_id()) return;

    playerData[JsonKeys::Volume] = status;
    update_status(Common::audioStatusFromString(status));
}

void AudioPlayer::audioVolumeChanged(QString playerid, double volume)
{
    if (playerid != get_id()) return;

    playerData[JsonKeys::Volume] = QString("%1").arg(volume);
    update_volume(volume);
}

void AudioPlayer::startPolling()
{
    //The timer is created once and reused, never deleted while it may be
    //active. Restarting it can never create a second polling chain.
    if (!pollTimer)
    {
        pollTimer = new QTimer(this);
        connect(pollTimer, &QTimer::timeout, this, [this]()
        {
            connection->queryState(QStringList(),
                                   QStringList(),
                                   QStringList() << get_id());
        });
    }

    if (!pollTimer->isActive())
        pollTimer->start(AudioPollIntervalMs);
}

void AudioPlayer::stopPolling()
{
    if (pollTimer)
        pollTimer->stop();
}

void AudioPlayer::audioCoverDownloaded(QString playerid, const QByteArray &data)
{
    if (playerid != get_id())
        return;

    const QImage cover = QImage::fromData(data);

    //Push a copy into the cache before advertising the new url, so that the
    //provider thread never reads a stale entry for a fresh url
    if (imageCache)
        imageCache->setImage(get_id(), cover);

    if (cover.isNull())
        update_cover({});
    else
        update_cover(QString("image://audio_cover/%1/%2")
                     .arg(get_id())
                     .arg(QRandomGenerator::global()->generate()));
}
