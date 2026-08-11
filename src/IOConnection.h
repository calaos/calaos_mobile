#ifndef IOCONNECTION_H
#define IOCONNECTION_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/* The slice of the server connection that the IO models actually use (T18).
 *
 * RoomModel/ScenarioModel/IOBase used to hold a CalaosConnection *, so
 * instantiating a single IO dragged in the whole transport: QNetworkAccessManager,
 * QWebSocket, the reconnection policy, the event decoder, HardwareUtils. They
 * need three things only, none of which is transport specific:
 *   - sending a command for an io,
 *   - knowing which shape the payloads follow (v2 HTTP API or the older one),
 *   - being told about the io changes pushed by the server.
 *
 * The dependency also used to run backwards: RoomModel::load() decided the API
 * version and wrote it into the connection through updateHttpApiV2(). That
 * detection now lives in CalaosConnection, next to the flag it sets, and the
 * interface below is read-only about it.
 *
 * This interface is deliberately NOT a QObject. Staying moc-free means it is a
 * plain header that any translation unit - including a test double that must
 * not link the transport - can implement without a meta-object of its own, and
 * without having to be listed in calaos.pri. The change notifications are
 * therefore reached through eventSource(), the QObject that really carries the
 * signals, and connected by name, the way most of this code base already does.
 *
 * eventSource() must emit, with exactly these signatures:
 *   void eventInputChange(QString id, QString key, QString value)
 *   void eventOutputChange(QString id, QString key, QString value)
 *   void eventIoStatusChange(QString id, QVariantMap statusData)
 * IOBase warns loudly if a connection fails, so a signature drift shows up as
 * a message instead of an io that silently stops updating.
 */
class IOConnection
{
public:
    virtual ~IOConnection() {}

    //The QObject emitting the three io signals above. Usually the implementer
    //itself; kept separate so implementers are free not to be QObjects.
    virtual QObject *eventSource() = 0;

    //True when the server payloads follow the v2 HTTP API shape: numeric rgb
    //states, rooms carrying separate "inputs"/"outputs" lists.
    virtual bool isHttpApiV2() const = 0;

    //type is "input"/"output" (or empty), action is typically "set_state".
    virtual void sendCommand(QString id, QString value, QString type, QString action) = 0;
};

#endif // IOCONNECTION_H
