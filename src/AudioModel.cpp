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

AudioPlayer::AudioPlayer(CalaosConnection *con):
    QStandardItem(),
    connection(con)
{
}

void AudioPlayer::load(QVariantMap &d)
{
    playerData = d;

    update_status(Common::audioStatusFromString(playerData["status"].toString()));
    update_id(playerData["player_id"].toString());
    update_cover(playerData["cover_url"].toString());
    update_name(playerData["name"].toString());
    update_volume(playerData["volume"].toDouble());

    QVariantMap currentTrack = playerData["current_track"].toMap();
    update_title(currentTrack["title"].toString());
    update_album(currentTrack["album"].toString());
    update_artist(currentTrack["artist"].toString());

    connect(connection, SIGNAL(eventAudioChange(QString)),
            this, SLOT(audioChanged(QString)));
    connect(connection, SIGNAL(eventAudioStatusChange(QString,QString)),
            this, SLOT(audioChangedStatus(QString,QString)));
    connect(connection, SIGNAL(eventAudioVolumeChange(QString,double)),
            this, SLOT(audioVolumeChanged(QString,double)));
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

void AudioPlayer::audioChange(QString playerid)
{
    if (playerid != playerData["player_id"].toString()) return;

    connection->queryState(QStringList(),
                           QStringList(),
                           QStringList() << playerid,
                           this,
                           "audioStateChange");
}

void AudioPlayer::audioStateChange(QVariantMap &data)
{
    QVariantList players = data["audio_players"].toList();
    QVariantList::iterator it = players.begin();
    for (;it != players.end();it++)
    {
        QVariantMap r = it->toMap();
        if (r["player_id"].toString() == playerData["player_id"].toString())
        {
            load(r);
            break;
        }
    }
}

void AudioPlayer::audioStatusChange(QString playerid, QString status)
{
    if (playerid != playerData["player_id"].toString()) return;

    playerData["volume"] = status;
    update_status(Common::audioStatusFromString(status));
}

void AudioPlayer::audioVolumeChange(QString playerid, double volume)
{
    if (playerid != playerData["player_id"].toString()) return;

    playerData["volume"] = QString("%1").arg(volume);
    update_volume(volume);
}
