#ifndef FAKETIMEDAEMON_H
#define FAKETIMEDAEMON_H

#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

//The box time routes of calaos-container in this process, as docs/API.md of
//that repository describes them. Used by tst_boxtime and tst_boxtimepage.
class FakeTimeDaemon: public QObject
{
public:
    struct Request
    {
        QByteArray method;
        QString path;
        QString query;
        QByteArray authorization;
        QByteArray body;
    };

    struct Answer
    {
        enum Kind { Reply, Drop, Hold };
        Kind kind = Reply;
        int status = 200;
        QByteArray body;
    };

    FakeTimeDaemon()
    {
        connect(&server, &QTcpServer::newConnection, this, [this]()
        {
            while (QTcpSocket *s = server.nextPendingConnection())
            {
                connect(s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead(s); });
                connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            }
        });
    }

    bool start() { return server.listen(QHostAddress::LocalHost); }
    void stop() { server.close(); }
    QString baseAddr() const { return QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()); }

    static QByteArray success(const QJsonObject &data)
    {
        return json(QJsonObject{{ "error", false }, { "msg", "ok" }, { "data", data }});
    }

    static QByteArray failure(const QString &code, const QString &msg, const QJsonObject &data = {})
    {
        QJsonObject o{{ "error", true }, { "code", code }, { "msg", msg }};
        if (!data.isEmpty())
            o["data"] = data;
        return json(o);
    }

    //the next call of this route gets this, before the normal behaviour comes back
    void script(const QByteArray &method, const QString &path, int status, const QByteArray &body)
    {
        scripted[routeKey(method, path)] << Answer{ Answer::Reply, status, body };
    }
    void scriptDrop(const QByteArray &method, const QString &path)
    {
        scripted[routeKey(method, path)] << Answer{ Answer::Drop, 0, {} };
    }
    //the next call waits for release(), and gets the state the box has now
    void scriptHold(const QByteArray &method, const QString &path)
    {
        scripted[routeKey(method, path)] << Answer{ Answer::Hold, 200, success(box) };
    }

    void release()
    {
        const auto list = held;
        held.clear();
        for (const auto &h: list)
            send(h.first, h.second.status, h.second.body);
    }

    QList<Request> requestsTo(const QByteArray &method, const QString &path) const
    {
        QList<Request> out;
        for (const Request &r: requests)
        {
            if (r.method == method && r.path == path)
                out << r;
        }
        return out;
    }

    QString token;
    //an old daemon: plain text 404 on all of /api/v2
    bool oldDaemon = false;
    //a daemon that has /api/v2 and not these routes: enveloped 404
    bool noTimeRoutes = false;

    QJsonObject box{
        { "now", "2026-10-25T09:12:03Z" },
        { "timezone", "Europe/Paris" },
        { "ntp", true },
        { "synchronized", true },
        { "rtc", QJsonValue::Null },
    };
    QStringList zones{ "America/New_York", "Asia/Tokyo", "Europe/Berlin", "Europe/Paris", "UTC" };
    QList<Request> requests;

private:
    static QByteArray json(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }

    static QByteArray routeKey(const QByteArray &method, const QString &path)
    {
        return method + ' ' + path.toUtf8();
    }

    static void send(QTcpSocket *s, int status, const QByteArray &body, const QByteArray &type = "application/json")
    {
        s->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\n"
                 "Content-Type: " + type + "\r\n"
                 "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
                 "Connection: close\r\n\r\n" + body);
        s->disconnectFromHost();
    }

    void onReadyRead(QTcpSocket *s)
    {
        const QByteArray raw = s->peek(s->bytesAvailable());
        const int headEnd = raw.indexOf("\r\n\r\n");
        if (headEnd < 0)
            return;

        Request req;
        int contentLength = 0;
        const QList<QByteArray> lines = raw.left(headEnd).split('\n');
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        const QUrl url(QString::fromLatin1(first.value(1)));
        req.method = first.value(0);
        req.path = url.path();
        req.query = url.query();
        for (const QByteArray &line: lines)
        {
            const int colon = line.indexOf(':');
            const QByteArray key = line.left(colon).trimmed().toLower();
            if (key == "authorization")
                req.authorization = line.mid(colon + 1).trimmed();
            else if (key == "content-length")
                contentLength = line.mid(colon + 1).trimmed().toInt();
        }

        if (raw.size() < headEnd + 4 + contentLength)
            return;
        s->read(headEnd + 4 + contentLength);
        req.body = raw.mid(headEnd + 4, contentLength);
        requests << req;
        route(s, req);
    }

    void route(QTcpSocket *s, const Request &req)
    {
        if (!token.isEmpty() && req.authorization != "Bearer " + token.toUtf8())
        {
            send(s, 401, failure("unauthorized", "wrong token"));
            return;
        }

        if (oldDaemon || !req.path.startsWith(QStringLiteral("/api/v2/")))
        {
            send(s, 404, "404 page not found\n", "text/plain");
            return;
        }

        const QByteArray key = routeKey(req.method, req.path.mid(7));
        if (!scripted[key].isEmpty())
        {
            const Answer answer = scripted[key].takeFirst();
            if (answer.kind == Answer::Drop)
                s->abort();
            else if (answer.kind == Answer::Hold)
                held << qMakePair(s, answer);
            else
                send(s, answer.status, answer.body);
            return;
        }

        if (noTimeRoutes && key != "POST /system/services/restart")
            send(s, 404, failure("not_found", "no such route"));
        else if (key == "GET /system/time")
            send(s, 200, success(box));
        else if (key == "POST /system/time")
            setTime(s, req);
        else if (key == "GET /system/timezones")
            send(s, 200, success({{ "timezones", QJsonArray::fromStringList(zones) }}));
        else if (key == "POST /system/services/restart")
            send(s, 200, success({{ "id", "calaos-server" }, { "state", "restarting" }}));
        else
            send(s, 404, failure("not_found", "no such route"));
    }

    void setTime(QTcpSocket *s, const Request &req)
    {
        const QJsonObject change = QJsonDocument::fromJson(req.body).object();
        const bool ntpAfter = change.contains("ntp") ? change.value("ntp").toBool() : box.value("ntp").toBool();
        if (change.contains("time") && ntpAfter)
        {
            send(s, 400, failure("invalid_request", "ntp is active"));
            return;
        }

        const QString zone = change.value("timezone").toString();
        if (change.contains("timezone") && !zones.contains(zone))
        {
            send(s, 400, failure("invalid_request", "unknown timezone"));
            return;
        }

        const bool zoneChanged = change.contains("timezone") && zone != box.value("timezone").toString();
        if (zoneChanged)
            box["timezone"] = zone;
        box["ntp"] = ntpAfter;
        if (change.contains("time"))
            box["now"] = change.value("time");

        QJsonObject data = box;
        data["restart_required"] = zoneChanged;
        send(s, 200, success(data));
    }

    QTcpServer server;
    QHash<QByteArray, QList<Answer>> scripted;
    QList<QPair<QTcpSocket *, Answer>> held;
};

#endif // FAKETIMEDAEMON_H
