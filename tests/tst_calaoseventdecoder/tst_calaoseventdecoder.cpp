#include <QtTest>
#include <QJsonDocument>
#include <QMetaEnum>
#include <QSet>

#include "CalaosEventDecoder.h"

//Unit tests of the event decoder extracted from CalaosConnection by T17.
//
//Everything here runs without a server, without a socket and without a single
//QObject of the application: the decoder takes the payload of a frame and
//returns the list of events it describes. That is exactly what could not be
//tested before, when the same rules lived inside the network slots.
//
//The frames used below are the ones calaos_server really sends on the two
//protocols the client speaks:
//  - v2 (HTTP long-poll): one space separated, percent encoded string per
//    event ("input <id> <state>:<value>").
//  - v3 (websocket and v3 HTTP API): one JSON object per event, carrying
//    "type_str" and "data".

namespace
{

//Every DecodedEvent::Type the test produced, so typeCoverageIsComplete() can
//fail the day a type is added to the enum and left untested.
QSet<int> g_seenTypes;

QList<DecodedEvent> record(const QList<DecodedEvent> &events)
{
    for (const DecodedEvent &ev: events)
        g_seenTypes.insert(int(ev.type));
    return events;
}

//Canonical one line form of an event, so a data driven failure names the
//offending field instead of printing a struct.
QString describe(const DecodedEvent &ev)
{
    const QMetaEnum me = QMetaEnum::fromType<DecodedEvent::Type>();
    const char *key = me.valueToKey(int(ev.type));

    const QStringList fields = { QString::fromLatin1(key ? key : "?"),
                                 ev.id,
                                 ev.state,
                                 ev.value,
                                 QString::number(ev.number),
                                 ev.rawType };
    return fields.join(QLatin1Char('|'));
}

QStringList describe(const QList<DecodedEvent> &events)
{
    QStringList out;
    for (const DecodedEvent &ev: events)
        out << describe(ev);
    return out;
}

//A frame as it comes off the wire: real JSON bytes, parsed the way
//CalaosConnection parses them before handing the payload to the decoder.
QVariantMap frameFromJson(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object().toVariantMap();
}

}

class TstCalaosEventDecoder: public QObject
{
    Q_OBJECT

private slots:
    void v2Events_data();
    void v2Events();
    void v2MalformedAndUnknown_data();
    void v2MalformedAndUnknown();
    void v2PercentEncodingIsDecoded();

    void v3Events_data();
    void v3Events();
    void v3IoChangedExpandsEveryState();
    void v3IoChangedWithoutIdIsNotFatal();
    void v3IoStatusCarriesTheWholeData();
    void v3MalformedAndUnknown_data();
    void v3MalformedAndUnknown();
    void v3AcceptsNumbersCarriedAsStrings();

    void apiVersionDispatch();
    void garbagePayloadsAreNotFatal_data();
    void garbagePayloadsAreNotFatal();

    void stateMapSplitsIoFromAudio();
    void stateMapStringModesDifferOnNumbers();

    void queryAnswerV2();
    void queryAnswerV3();
    void queryAnswerReportsEveryPayloadOfTheSameAnswer();
    void queryAnswerIgnoresEmptyLists();

    void typeCoverageIsComplete();
};

//----------------------------------------------------------------------------
// v2: space separated strings
//----------------------------------------------------------------------------

void TstCalaosEventDecoder::v2Events_data()
{
    QTest::addColumn<QString>("frame");
    QTest::addColumn<QStringList>("expected");

    //State only, no value after the colon.
    QTest::newRow("input state only")
            << "input input_0 true"
            << QStringList{ "InputChange|input_0|true||0|" };

    QTest::newRow("output state only")
            << "output output_2 false"
            << QStringList{ "OutputChange|output_2|false||0|" };

    //The third field is "<state>:<value>", the shape the shutters and the
    //dimmers use.
    QTest::newRow("output state and value")
            << "output output_3 state%3A75"
            << QStringList{ "OutputChange|output_3|state|75|0|" };

    //Extra fields after the third one are ignored, they always were.
    QTest::newRow("trailing fields ignored")
            << "input input_1 12.5 something else"
            << QStringList{ "InputChange|input_1|12.5||0|" };

    QTest::newRow("audio_volume")
            << "audio_volume 0 volume 42"
            << QStringList{ "AudioVolumeChange|0|||42|" };

    QTest::newRow("audio_volume decimal")
            << "audio_volume player_1 volume 12.5"
            << QStringList{ "AudioVolumeChange|player_1|||12.5|" };

    QTest::newRow("audio_status")
            << "audio_status 0 play"
            << QStringList{ "AudioStatusChange|0|play||0|" };

    QTest::newRow("audio_status pause")
            << "audio_status 0 pause"
            << QStringList{ "AudioStatusChange|0|pause||0|" };

    QTest::newRow("audio songchanged")
            << "audio 0 songchanged"
            << QStringList{ "AudioSongChange|0|||0|" };
}

void TstCalaosEventDecoder::v2Events()
{
    QFETCH(QString, frame);
    QFETCH(QStringList, expected);

    QCOMPARE(describe(record(CalaosEventDecoder::decodeEventV2(frame))), expected);
}

void TstCalaosEventDecoder::v2MalformedAndUnknown_data()
{
    QTest::addColumn<QString>("frame");
    QTest::addColumn<QStringList>("expected");

    //An empty frame is not an event at all: the long-poll answer can carry
    //empty strings and they must decode to nothing.
    QTest::newRow("empty") << QString() << QStringList{};
    QTest::newRow("empty string") << "" << QStringList{};

    //Truncations. None of them may index past the end of the split.
    QTest::newRow("input truncated")
            << "input input_0"
            << QStringList{ "Malformed||||0|input" };
    QTest::newRow("output truncated")
            << "output"
            << QStringList{ "Malformed||||0|output" };
    QTest::newRow("audio_volume truncated")
            << "audio_volume 0 volume"
            << QStringList{ "Malformed||||0|audio_volume" };

    //The guard added by T08: audio_status used to read spl.at(2) blindly.
    QTest::newRow("audio_status truncated")
            << "audio_status 0"
            << QStringList{ "Malformed||||0|audio_status" };
    QTest::newRow("audio_status alone")
            << "audio_status"
            << QStringList{ "Malformed||||0|audio_status" };

    //Event types the UI does not consume: dropped, never an error.
    QTest::newRow("audio playlist")
            << "audio 0 playlistchanged"
            << QStringList{ "Unknown||||0|audio" };
    QTest::newRow("audio truncated is unknown")
            << "audio 0"
            << QStringList{ "Unknown||||0|audio" };
    QTest::newRow("unhandled type")
            << "scenario_added scenario_12"
            << QStringList{ "Unknown||||0|scenario_added" };
    QTest::newRow("garbage")
            << "}{ not a frame"
            << QStringList{ "Unknown||||0|}{" };
}

void TstCalaosEventDecoder::v2MalformedAndUnknown()
{
    QFETCH(QString, frame);
    QFETCH(QStringList, expected);

    const QList<DecodedEvent> events = record(CalaosEventDecoder::decodeEventV2(frame));
    QCOMPARE(describe(events), expected);

    //Nothing decoded here may ever reach the models.
    for (const DecodedEvent &ev: events)
        QVERIFY(!ev.valid());
}

void TstCalaosEventDecoder::v2PercentEncodingIsDecoded()
{
    //Real ids contain spaces and accents, and the v2 wire format percent
    //encodes both the id and the state. The expected id is spelled from its
    //UTF-8 bytes to keep this source file pure ASCII.
    const QList<DecodedEvent> events =
            record(CalaosEventDecoder::decodeEventV2("input Salon%20Lumi%C3%A8re on%3A100"));

    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).type, DecodedEvent::InputChange);
    QCOMPARE(events.at(0).id, QString::fromUtf8("Salon Lumi\xC3\xA8re"));
    QCOMPARE(events.at(0).state, QStringLiteral("on"));
    QCOMPARE(events.at(0).value, QStringLiteral("100"));
    QVERIFY(events.at(0).valid());
}

//----------------------------------------------------------------------------
// v3: JSON objects
//----------------------------------------------------------------------------

void TstCalaosEventDecoder::v3Events_data()
{
    QTest::addColumn<QByteArray>("frame");
    QTest::addColumn<QStringList>("expected");

    //An io_changed frame is reported both as an input and as an output
    //change: the client does not know on which side the id lives.
    QTest::newRow("io_changed")
            << QByteArray(R"({"type":3,"type_str":"io_changed",
                              "data":{"id":"output_1","state":"true"}})")
            << QStringList{ "InputChange|output_1|state|true|0|",
                            "OutputChange|output_1|state|true|0|" };

    QTest::newRow("audio_volume_changed")
            << QByteArray(R"({"type":6,"type_str":"audio_volume_changed",
                              "data":{"player_id":"0","volume":42}})")
            << QStringList{ "AudioVolumeChange|0|||42|" };

    QTest::newRow("audio_status_changed")
            << QByteArray(R"({"type":5,"type_str":"audio_status_changed",
                              "data":{"player_id":"0","state":"play"}})")
            << QStringList{ "AudioStatusChange|0|play||0|" };

    QTest::newRow("audio_song_changed")
            << QByteArray(R"({"type":7,"type_str":"audio_song_changed",
                              "data":{"player_id":"1"}})")
            << QStringList{ "AudioSongChange|1|||0|" };

    QTest::newRow("touchscreen_camera_request")
            << QByteArray(R"({"type":20,"type_str":"touchscreen_camera_request",
                              "data":{"id":"camera_0"}})")
            << QStringList{ "TouchscreenCamera|camera_0|||0|" };

    QTest::newRow("io_status_changed")
            << QByteArray(R"({"type":21,"type_str":"io_status_changed",
                              "data":{"id":"output_4","status":"error"}})")
            << QStringList{ "IoStatusChange|output_4|||0|" };
}

void TstCalaosEventDecoder::v3Events()
{
    QFETCH(QByteArray, frame);
    QFETCH(QStringList, expected);

    QCOMPARE(describe(record(CalaosEventDecoder::decodeEventV3(frameFromJson(frame)))), expected);
}

void TstCalaosEventDecoder::v3IoChangedExpandsEveryState()
{
    //One frame carries every state that changed on the IO. Each of them must
    //produce its own input/output pair, in key order.
    const QList<DecodedEvent> events = record(CalaosEventDecoder::decodeEventV3(
        frameFromJson(R"({"type_str":"io_changed",
                          "data":{"id":"output_9","state":"true","value":"75"}})")));

    QCOMPARE(describe(events), (QStringList{ "InputChange|output_9|state|true|0|",
                                             "OutputChange|output_9|state|true|0|",
                                             "InputChange|output_9|value|75|0|",
                                             "OutputChange|output_9|value|75|0|" }));
}

void TstCalaosEventDecoder::v3IoChangedWithoutIdIsNotFatal()
{
    //The id used to be read with a non const operator[] from inside the loop
    //that iterates the very same map: a frame without "id" inserted a key
    //while iterating. Now it decodes to events with an empty id.
    const QList<DecodedEvent> events = record(CalaosEventDecoder::decodeEventV3(
        frameFromJson(R"({"type_str":"io_changed","data":{"state":"true"}})")));

    QCOMPARE(describe(events), (QStringList{ "InputChange||state|true|0|",
                                             "OutputChange||state|true|0|" }));

    //And a frame with no state at all decodes to nothing.
    QCOMPARE(record(CalaosEventDecoder::decodeEventV3(
                 frameFromJson(R"({"type_str":"io_changed","data":{"id":"output_1"}})"))).size(), 0);
}

void TstCalaosEventDecoder::v3IoStatusCarriesTheWholeData()
{
    //eventIoStatusChange() forwards the whole data map to the models, not
    //just the id.
    const QList<DecodedEvent> events = record(CalaosEventDecoder::decodeEventV3(
        frameFromJson(R"({"type_str":"io_status_changed",
                          "data":{"id":"output_4","status":"error","reason":"timeout"}})")));

    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).id, QStringLiteral("output_4"));
    QCOMPARE(events.at(0).data.value("status").toString(), QStringLiteral("error"));
    QCOMPARE(events.at(0).data.value("reason").toString(), QStringLiteral("timeout"));
    QCOMPARE(events.at(0).data.value("id").toString(), QStringLiteral("output_4"));
}

void TstCalaosEventDecoder::v3MalformedAndUnknown_data()
{
    QTest::addColumn<QByteArray>("frame");
    QTest::addColumn<QStringList>("expected");

    //io_status_changed without an id has nothing to address.
    QTest::newRow("io_status without id")
            << QByteArray(R"({"type_str":"io_status_changed","data":{"status":"error"}})")
            << QStringList{ "Malformed||||0|io_status_changed" };

    //Event types the server sends and the UI does not consume.
    QTest::newRow("push notification")
            << QByteArray(R"({"type":22,"type_str":"push_notification","data":{"body":"hello"}})")
            << QStringList{ "Unknown||||0|push_notification" };
    QTest::newRow("room_changed")
            << QByteArray(R"({"type_str":"room_changed","data":{"name":"Salon"}})")
            << QStringList{ "Unknown||||0|room_changed" };

    //Shapes that must not crash: no type_str, no data, wrong types.
    QTest::newRow("no type_str")
            << QByteArray(R"({"data":{"id":"output_1"}})")
            << QStringList{ "Unknown||||0|" };
    QTest::newRow("empty object")
            << QByteArray("{}")
            << QStringList{ "Unknown||||0|" };
    QTest::newRow("data is not an object")
            << QByteArray(R"({"type_str":"io_changed","data":"nonsense"})")
            << QStringList{};
    QTest::newRow("data missing")
            << QByteArray(R"({"type_str":"audio_status_changed"})")
            << QStringList{ "AudioStatusChange||||0|" };
    QTest::newRow("invalid json")
            << QByteArray("{not json at all")
            << QStringList{ "Unknown||||0|" };
}

void TstCalaosEventDecoder::v3MalformedAndUnknown()
{
    QFETCH(QByteArray, frame);
    QFETCH(QStringList, expected);

    QCOMPARE(describe(record(CalaosEventDecoder::decodeEventV3(frameFromJson(frame)))), expected);
}

void TstCalaosEventDecoder::v3AcceptsNumbersCarriedAsStrings()
{
    //The server carries the volume as a JSON number on some builds and as a
    //string on others. Common::toDoubleSafe() (T11) handles both and never
    //turns garbage into a silent 0 without saying so.
    const QList<DecodedEvent> asString = record(CalaosEventDecoder::decodeEventV3(
        frameFromJson(R"({"type_str":"audio_volume_changed",
                          "data":{"player_id":"0","volume":"37.5"}})")));

    QCOMPARE(asString.size(), 1);
    QCOMPARE(asString.at(0).number, 37.5);
}

//----------------------------------------------------------------------------
// Entry point used by the long-poll loop
//----------------------------------------------------------------------------

void TstCalaosEventDecoder::apiVersionDispatch()
{
    //This is what the long-poll does with every element of the "events" list:
    //the api version decides how the QVariant is read.
    const QVariant v2 = QVariant(QStringLiteral("audio_status 0 play"));
    QCOMPARE(describe(record(CalaosEventDecoder::decodeEvent(v2, CalaosEventDecoder::ApiV2))),
             (QStringList{ "AudioStatusChange|0|play||0|" }));

    const QVariant v3 = QVariant(frameFromJson(
        R"({"type_str":"audio_status_changed","data":{"player_id":"0","state":"stop"}})"));
    QCOMPARE(describe(record(CalaosEventDecoder::decodeEvent(v3, CalaosEventDecoder::ApiV3))),
             (QStringList{ "AudioStatusChange|0|stop||0|" }));
}

void TstCalaosEventDecoder::garbagePayloadsAreNotFatal_data()
{
    QTest::addColumn<QVariant>("payload");
    QTest::addColumn<int>("version");
    QTest::addColumn<int>("count");

    //A v3 payload read as v2 and the other way round: neither may crash.
    QTest::newRow("map as v2") << QVariant(QVariantMap{{ "type_str", "io_changed" }})
                               << int(CalaosEventDecoder::ApiV2) << 0;
    QTest::newRow("string as v3") << QVariant(QStringLiteral("input input_0 true"))
                                  << int(CalaosEventDecoder::ApiV3) << 1;
    QTest::newRow("number as v2") << QVariant(42) << int(CalaosEventDecoder::ApiV2) << 1;
    QTest::newRow("number as v3") << QVariant(42) << int(CalaosEventDecoder::ApiV3) << 1;
    QTest::newRow("null as v2") << QVariant() << int(CalaosEventDecoder::ApiV2) << 0;
    QTest::newRow("null as v3") << QVariant() << int(CalaosEventDecoder::ApiV3) << 1;
    QTest::newRow("list as v3") << QVariant(QVariantList{ 1, 2 })
                                << int(CalaosEventDecoder::ApiV3) << 1;
}

void TstCalaosEventDecoder::garbagePayloadsAreNotFatal()
{
    QFETCH(QVariant, payload);
    QFETCH(int, version);
    QFETCH(int, count);

    const QList<DecodedEvent> events =
            record(CalaosEventDecoder::decodeEvent(payload, CalaosEventDecoder::ApiVersion(version)));

    QCOMPARE(events.size(), count);
    for (const DecodedEvent &ev: events)
        QVERIFY(!ev.valid());
}

//----------------------------------------------------------------------------
// get_state answers
//----------------------------------------------------------------------------

void TstCalaosEventDecoder::stateMapSplitsIoFromAudio()
{
    //Answer of a get_state over the websocket: io states are strings, audio
    //players are objects. Keys come out sorted, "0" before "output_1".
    const QVariantMap states = frameFromJson(
        R"({"output_1":"true","0":{"status":"play","volume":42}})");

    const QList<DecodedEvent> events =
            record(CalaosEventDecoder::decodeStateMap(states, CalaosEventDecoder::StrictString));

    QCOMPARE(describe(events), (QStringList{ "AudioStateChange|0|||0|",
                                             "InputStateChange||||0|",
                                             "OutputStateChange||||0|" }));

    QCOMPARE(events.at(0).data.value("status").toString(), QStringLiteral("play"));
    //The io state is repacked into the { id, state } map the models expect.
    QCOMPARE(events.at(1).data.value("id").toString(), QStringLiteral("output_1"));
    QCOMPARE(events.at(1).data.value("state").toString(), QStringLiteral("true"));
    QCOMPARE(events.at(2).data, events.at(1).data);
}

void TstCalaosEventDecoder::stateMapStringModesDifferOnNumbers()
{
    //Documented divergence between the two transports, preserved by T17: a
    //JSON number is an io state over HTTP and an (empty) audio payload over
    //the websocket. Unifying them would change what the models receive.
    const QVariantMap states = frameFromJson(R"({"input_1":42})");

    QCOMPARE(describe(record(CalaosEventDecoder::decodeStateMap(states, CalaosEventDecoder::StrictString))),
             (QStringList{ "AudioStateChange|input_1|||0|" }));

    QCOMPARE(describe(record(CalaosEventDecoder::decodeStateMap(states, CalaosEventDecoder::ConvertibleString))),
             (QStringList{ "InputStateChange||||0|", "OutputStateChange||||0|" }));

    //An object is an audio payload in both modes.
    const QVariantMap audio = frameFromJson(R"({"0":{"status":"pause"}})");
    QCOMPARE(describe(record(CalaosEventDecoder::decodeStateMap(audio, CalaosEventDecoder::StrictString))),
             describe(record(CalaosEventDecoder::decodeStateMap(audio, CalaosEventDecoder::ConvertibleString))));
}

//----------------------------------------------------------------------------
// HTTP query answers
//----------------------------------------------------------------------------

void TstCalaosEventDecoder::queryAnswerV2()
{
    //v2 forwards the whole answer to the models, which know how to read the
    //"inputs" and "outputs" lists.
    const QVariantMap answer = frameFromJson(
        R"({"inputs":[{"id":"input_1","state":"true"}],
            "outputs":[{"id":"output_1","state":"false"}]})");

    const QList<DecodedEvent> events =
            record(CalaosEventDecoder::decodeQueryAnswer(answer, CalaosEventDecoder::ApiV2));

    QCOMPARE(describe(events), (QStringList{ "InputStateChange||||0|",
                                             "OutputStateChange||||0|" }));
    QCOMPARE(events.at(0).data, answer);
    QCOMPARE(events.at(1).data, answer);
}

void TstCalaosEventDecoder::queryAnswerV3()
{
    //v3 answers are a flat map of id -> state, read with the permissive
    //string mode.
    const QVariantMap answer = frameFromJson(R"({"input_1":"true","output_1":"false"})");

    QCOMPARE(describe(record(CalaosEventDecoder::decodeQueryAnswer(answer, CalaosEventDecoder::ApiV3))),
             (QStringList{ "InputStateChange||||0|", "OutputStateChange||||0|",
                           "InputStateChange||||0|", "OutputStateChange||||0|" }));
}

void TstCalaosEventDecoder::queryAnswerReportsEveryPayloadOfTheSameAnswer()
{
    //Audio players and an event log page can share one answer, and the order
    //the previous implementation emitted them in is part of the contract.
    const QVariantMap answer = frameFromJson(
        R"({"audio_players":[{"id":"0"}],
            "events":["e1"],
            "total_page":3,
            "data":{"events":[{"msg":"hello"}]},
            "inputs":[{"id":"input_1"}]})");

    const QList<DecodedEvent> events =
            record(CalaosEventDecoder::decodeQueryAnswer(answer, CalaosEventDecoder::ApiV2));

    QCOMPARE(describe(events), (QStringList{ "AudioStateChange||||0|",
                                             "LogEvent||||0|",
                                             "InputStateChange||||0|" }));

    //The audio payload is the whole answer, the log payload is answer["data"].
    QCOMPARE(events.at(0).data, answer);
    QVERIFY(events.at(1).data.contains("events"));
    QCOMPARE(events.at(1).data, answer.value("data").toMap());
}

void TstCalaosEventDecoder::queryAnswerIgnoresEmptyLists()
{
    //An answer that only carries empty lists is not a state change.
    const QVariantMap answer = frameFromJson(
        R"({"audio_players":[],"inputs":[],"outputs":[]})");

    QCOMPARE(record(CalaosEventDecoder::decodeQueryAnswer(answer, CalaosEventDecoder::ApiV2)).size(), 0);

    //And an event list without its page count is not an event log page.
    const QVariantMap noPage = frameFromJson(R"({"events":["e1"],"data":{"x":1}})");
    QCOMPARE(record(CalaosEventDecoder::decodeQueryAnswer(noPage, CalaosEventDecoder::ApiV2)).size(), 0);
}

//----------------------------------------------------------------------------

void TstCalaosEventDecoder::typeCoverageIsComplete()
{
    //Iterating the QMetaEnum rather than a hand written list: a
    //DecodedEvent::Type added later fails this test until a case above
    //produces it. Runs last on purpose, it reads what the other slots saw.
    const QMetaEnum me = QMetaEnum::fromType<DecodedEvent::Type>();
    QVERIFY(me.isValid());

    QStringList missing;
    for (int i = 0; i < me.keyCount(); i++)
    {
        if (!g_seenTypes.contains(me.value(i)))
            missing << QString::fromLatin1(me.key(i));
    }

    QVERIFY2(missing.isEmpty(),
             qPrintable(QStringLiteral("DecodedEvent::Type values never decoded by this test: %1")
                        .arg(missing.join(QStringLiteral(", ")))));
}

QTEST_APPLESS_MAIN(TstCalaosEventDecoder)
#include "tst_calaoseventdecoder.moc"
