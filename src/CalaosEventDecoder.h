#ifndef CALAOSEVENTDECODER_H
#define CALAOSEVENTDECODER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QVariantMap>

//Pure decoding of the Calaos event and state payloads (T17).
//
//No QObject instance, no network, no HardwareUtils: given the payload of a
//frame the transport already read and parsed, these functions return the list
//of events it describes. CalaosConnection keeps the transport and the job of
//turning that list into its existing signals.
//
//That split is what makes the protocol testable without a server: the test
//(tests/tst_calaoseventdecoder) links this file and Common.cpp, nothing else.
//
//The rules below are a straight move of the ones that used to live in
//CalaosConnection::processEventsV2() / processEventsV3() / requestFinished()
//and in the "get_state" branch of onWsTextMessageReceived(), guards included.

struct DecodedEvent
{
    Q_GADGET
public:
    enum Type
    {
        //The frame carries an event type this client does not consume. Not an
        //error: calaos_server emits more event types than the UI uses. This is
        //what the two "TODO all other event types" comments used to drop
        //silently.
        Unknown = 0,

        //The frame is one we do handle, but it is truncated or has the wrong
        //shape. Dropped without ever being fatal.
        Malformed,

        InputChange,        //id, state, value
        OutputChange,       //id, state, value
        AudioVolumeChange,  //id = player id, number = volume
        AudioStatusChange,  //id = player id, state = status
        AudioSongChange,    //id = player id
        TouchscreenCamera,  //id = camera id
        IoStatusChange,     //id, data
        InputStateChange,   //data
        OutputStateChange,  //data
        AudioStateChange,   //id = player id (may be empty), data
        LogEvent,           //data
    };
    Q_ENUM(Type)

    Type type = Unknown;

    //IO id, audio player id or camera id, depending on type.
    QString id;
    //State name for an IO change, status string for an audio status change.
    QString state;
    //Value of an IO change, empty when the wire carried none.
    QString value;
    //Numeric payload (audio volume only).
    double number = 0.0;
    //Map payload for the events whose signal carries a whole map.
    QVariantMap data;
    //Event type as it came from the wire. Only filled for Unknown and
    //Malformed, so a caller (or a test) can tell what was dropped.
    QString rawType;

    bool valid() const { return type != Unknown && type != Malformed; }
};

class CalaosEventDecoder
{
public:
    enum ApiVersion
    {
        //Long-poll answers of the v2 HTTP API: one event per space separated
        //string, with percent encoded fields ("input <id> <state>:<value>").
        ApiV2,
        //Websocket and v3 HTTP API: one event per JSON object carrying
        //"type_str" and "data".
        ApiV3,
    };

    //How a scalar value is told apart from an audio player payload in a
    //"get_state" style map. The two transports historically disagree and the
    //difference is preserved on purpose (see decodeStateMap()).
    enum StateStringMode
    {
        //Websocket: only a real JSON string is a scalar state. A number is an
        //audio player payload (and yields an empty map).
        StrictString,
        //HTTP: anything convertible to a string is a scalar state, which
        //includes numbers and booleans.
        ConvertibleString,
    };

    //One event of a long-poll answer. v is what isHttpApiV2() decides:
    //ApiV2 reads the QVariant as a string, ApiV3 as a map.
    static QList<DecodedEvent> decodeEvent(const QVariant &event, ApiVersion v);

    //v2 wire format: "<what> <arg1> <arg2> ...", every field percent encoded.
    //An empty message decodes to nothing at all.
    static QList<DecodedEvent> decodeEventV2(const QString &msg);

    //v3 wire format: { "type_str": ..., "data": { ... } }. A single frame can
    //describe several events (one io_changed frame carries every changed
    //state of an IO).
    static QList<DecodedEvent> decodeEventV3(const QVariantMap &msg);

    //Body of a "get_state" answer: a map of io id -> state string, mixed with
    //player id -> audio player map.
    static QList<DecodedEvent> decodeStateMap(const QVariantMap &states, StateStringMode mode);

    //Whole answer of an HTTP request (get_state, get_eventlog, audio query):
    //several unrelated payloads can share one answer, and all of them are
    //reported, in the order the previous implementation emitted them.
    static QList<DecodedEvent> decodeQueryAnswer(const QVariantMap &answer, ApiVersion v);
};

#endif // CALAOSEVENTDECODER_H
