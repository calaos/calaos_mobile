#ifndef SERVERCAPABILITIES_H
#define SERVERCAPABILITIES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

//What calaos_server says it can do (message get_server_info, calaos_base
//docs/08_http_api.md). A server that does not answer, or whose answer lacks a
//capability, is an old server: the client keeps the old behaviour. Header-only
//and free of the transport so the rules are testable on their own.
struct ServerCapabilities
{
    //Unknown: not asked on this connection. Pending: asked, waiting.
    //Answered: the list below is the server's. Unanswered: timeout or HTTP-only
    //login; nothing is announced.
    enum State { Unknown, Pending, Answered, Unanswered };

    //Capability names, as the server spells them.
    static inline const QString CameraToken = QStringLiteral("camera_token");
    static inline const QString AdminRelay = QStringLiteral("admin_relay");

    State state = Unknown;
    QString version;
    QStringList capabilities;

    bool has(const QString &name) const
    {
        return state == Answered && capabilities.contains(name);
    }

    //A reply with no usable list is still an answer: an empty one.
    static ServerCapabilities fromReply(const QJsonObject &data)
    {
        ServerCapabilities c;
        c.state = Answered;
        c.version = data.value(QStringLiteral("version")).toString();
        const QJsonValue list = data.value(QStringLiteral("capabilities"));
        if (list.isArray())
        {
            for (const QJsonValue &v: list.toArray())
            {
                if (v.isString())
                    c.capabilities << v.toString();
            }
        }
        return c;
    }
};

#endif // SERVERCAPABILITIES_H
