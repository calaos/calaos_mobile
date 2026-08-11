#include "CalaosEventDecoder.h"
#include "Common.h"

#include <QUrl>
#include <QDebug>

namespace
{

DecodedEvent makeEvent(DecodedEvent::Type type)
{
    DecodedEvent ev;
    ev.type = type;
    return ev;
}

DecodedEvent makeUnknown(DecodedEvent::Type type, const QString &rawType)
{
    DecodedEvent ev;
    ev.type = type;
    ev.rawType = rawType;
    return ev;
}

//An IO state change (v2 "input"/"output" and v3 "io_changed"): the same three
//fields feed either eventInputChange() or eventOutputChange().
DecodedEvent makeIoChange(DecodedEvent::Type type, const QString &id,
                          const QString &state, const QString &value)
{
    DecodedEvent ev;
    ev.type = type;
    ev.id = id;
    ev.state = state;
    ev.value = value;
    return ev;
}

DecodedEvent makeMapEvent(DecodedEvent::Type type, const QString &id, const QVariantMap &data)
{
    DecodedEvent ev;
    ev.type = type;
    ev.id = id;
    ev.data = data;
    return ev;
}

}

QList<DecodedEvent> CalaosEventDecoder::decodeEvent(const QVariant &event, ApiVersion v)
{
    if (v == ApiV2)
        return decodeEventV2(event.toString());

    return decodeEventV3(event.toMap());
}

QList<DecodedEvent> CalaosEventDecoder::decodeEventV2(const QString &msg)
{
    QList<DecodedEvent> events;

    if (msg == "") return events;

    const QStringList spl = msg.split(' ');
    const QString what = spl.at(0);

    if (what == "output" || what == "input")
    {
        if (spl.size() < 3)
        {
            events << makeUnknown(DecodedEvent::Malformed, what);
            return events;
        }

        const QString id = QUrl::fromPercentEncoding(spl.at(1).toLocal8Bit());
        const QStringList s = QUrl::fromPercentEncoding(spl.at(2).toLocal8Bit()).split(':');
        QString val;
        if (s.size() > 1) val = s.at(1);

        events << makeIoChange(what == "input" ? DecodedEvent::InputChange
                                               : DecodedEvent::OutputChange,
                               id, s.at(0), val);
    }
    else if (what == "audio_volume")
    {
        if (spl.count() < 4)
        {
            events << makeUnknown(DecodedEvent::Malformed, what);
            return events;
        }

        DecodedEvent ev = makeEvent(DecodedEvent::AudioVolumeChange);
        ev.id = spl.at(1);
        ev.number = Common::toDoubleSafe(spl.at(3), 0.0, "audio_volume event");
        events << ev;
    }
    else if (what == "audio_status")
    {
        //Size guard added by T08: a truncated audio_status used to index past
        //the end of the split.
        if (spl.count() < 3)
        {
            qWarning() << "Malformed audio_status event: " << msg;
            events << makeUnknown(DecodedEvent::Malformed, what);
            return events;
        }

        DecodedEvent ev = makeEvent(DecodedEvent::AudioStatusChange);
        ev.id = spl.at(1);
        ev.state = spl.at(2);
        events << ev;
    }
    else if (what == "audio")
    {
        if (spl.count() > 2 &&
            spl.at(2) == "songchanged")
        {
            DecodedEvent ev = makeEvent(DecodedEvent::AudioSongChange);
            ev.id = spl.at(1);
            events << ev;
        }
        else
        {
            //Every other "audio ..." sub event is one we do not consume.
            events << makeUnknown(DecodedEvent::Unknown, what);
        }
    }
    else
    {
        //All the other event types the server can send.
        events << makeUnknown(DecodedEvent::Unknown, what);
    }

    return events;
}

QList<DecodedEvent> CalaosEventDecoder::decodeEventV3(const QVariantMap &msg)
{
    QList<DecodedEvent> events;

    const QString typeStr = msg["type_str"].toString();
    const QVariantMap data = msg["data"].toMap();

    if (typeStr == "io_changed")
    {
        //One frame carries every state of the IO that changed. Each of them is
        //reported as an input *and* as an output change: the client does not
        //know on which side the id lives, the models sort it out.
        const QString id = data["id"].toString();
        for (auto it = data.constBegin();it != data.constEnd();it++)
        {
            if (it.key() == "id") continue;
            events << makeIoChange(DecodedEvent::InputChange, id, it.key(), it.value().toString());
            events << makeIoChange(DecodedEvent::OutputChange, id, it.key(), it.value().toString());
        }
    }
    else if (typeStr == "audio_volume_changed")
    {
        DecodedEvent ev = makeEvent(DecodedEvent::AudioVolumeChange);
        ev.id = data["player_id"].toString();
        ev.number = Common::toDoubleSafe(data["volume"], 0.0, "audio_volume_changed.volume");
        events << ev;
    }
    else if (typeStr == "audio_status_changed")
    {
        DecodedEvent ev = makeEvent(DecodedEvent::AudioStatusChange);
        ev.id = data["player_id"].toString();
        ev.state = data["state"].toString();
        events << ev;
    }
    else if (typeStr == "audio_song_changed")
    {
        DecodedEvent ev = makeEvent(DecodedEvent::AudioSongChange);
        ev.id = data["player_id"].toString();
        events << ev;
    }
    else if (typeStr == "touchscreen_camera_request")
    {
        DecodedEvent ev = makeEvent(DecodedEvent::TouchscreenCamera);
        ev.id = data["id"].toString();
        events << ev;
    }
    else if (typeStr == "io_status_changed")
    {
        if (data.contains("id"))
            events << makeMapEvent(DecodedEvent::IoStatusChange, data["id"].toString(), data);
        else
            events << makeUnknown(DecodedEvent::Malformed, typeStr);
    }
    else
    {
        //All the other event types the server can send.
        events << makeUnknown(DecodedEvent::Unknown, typeStr);
    }

    return events;
}

QList<DecodedEvent> CalaosEventDecoder::decodeStateMap(const QVariantMap &states, StateStringMode mode)
{
    QList<DecodedEvent> events;

    for (auto it = states.constBegin();it != states.constEnd();it++)
    {
        //The two transports disagree on what a scalar state is, and they
        //always have: over the websocket a JSON number is *not* a state and
        //ends up in the audio branch, over HTTP it is. Unifying them here
        //would silently change what the models receive, so the caller picks.
        const bool isScalar = mode == StrictString
                              ? it.value().typeId() == QMetaType::QString
                              : it.value().canConvert<QString>();

        if (isScalar)
        {
            const QVariantMap m = { { "id", it.key() },
                                    { "state", it.value().toString() }};
            events << makeMapEvent(DecodedEvent::InputStateChange, QString(), m);
            events << makeMapEvent(DecodedEvent::OutputStateChange, QString(), m);
        }
        else
        {
            events << makeMapEvent(DecodedEvent::AudioStateChange, it.key(), it.value().toMap());
        }
    }

    return events;
}

QList<DecodedEvent> CalaosEventDecoder::decodeQueryAnswer(const QVariantMap &answer, ApiVersion v)
{
    QList<DecodedEvent> events;

    if (answer.contains("audio_players") &&
        !answer["audio_players"].toList().isEmpty())
    {
        //Audio player states come back as a whole, with no player id.
        events << makeMapEvent(DecodedEvent::AudioStateChange, QString(), answer);
    }

    if (answer.contains("events") &&
        answer.contains("total_page"))
    {
        events << makeMapEvent(DecodedEvent::LogEvent, QString(), answer["data"].toMap());
    }

    if (v == ApiV2)
    {
        if (answer.contains("inputs") &&
            !answer["inputs"].toList().isEmpty())
            events << makeMapEvent(DecodedEvent::InputStateChange, QString(), answer);

        if (answer.contains("outputs") &&
            !answer["outputs"].toList().isEmpty())
            events << makeMapEvent(DecodedEvent::OutputStateChange, QString(), answer);
    }
    else
    {
        events += decodeStateMap(answer, ConvertibleString);
    }

    return events;
}
