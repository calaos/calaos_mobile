#ifndef ADMINTRANSPORT_H
#define ADMINTRANSPORT_H

#include <QJsonObject>
#include <QString>

//What AdminRelay needs from the household connection (CalaosConnection),
//behind an interface so the relay can be driven without a socket.
class AdminTransport
{
public:
    virtual ~AdminTransport() = default;
    //One frame on the logged-in WebSocket; false when none is up (nothing sent).
    virtual bool sendAdminMessage(const QString &msg, const QJsonObject &data, const QString &msgId) = 0;
    //A certificate question is waiting for the user.
    virtual bool hasPendingTlsTrust() const = 0;
    //The logged-in WebSocket is TLS (wss): the only one the admin password may use.
    virtual bool isSecure() const = 0;
    //Logged in over the HTTP fallback: admin messages have no transport.
    virtual bool isHttpFallback() const = 0;
    //host:port of the WebSocket, as the TLS pins name it, logged in or not;
    //empty without one. What is stored for a box is stored under this.
    virtual QString adminHostKey() const = 0;
};

#endif // ADMINTRANSPORT_H
