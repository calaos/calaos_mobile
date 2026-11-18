#ifndef PROGRAMTRANSPORT_H
#define PROGRAMTRANSPORT_H

#include <QJsonObject>
#include <QString>

//What ProgramClient needs from the household connection (CalaosConnection),
//behind an interface so the client can be driven without a socket. The
//implementation also carries two signals, connected by name:
//trackedRequestFinished(QString requestId, bool transportOk, QVariantMap answer)
//and eventTimeRangeChange(QString id).
class ProgramTransport
{
public:
    virtual ~ProgramTransport() = default;
    //Sends one request whose answer is tied back to it. Empty when nothing
    //was sent (no session up): no answer will come.
    virtual QString sendTrackedRequest(const QString &action, QJsonObject data) = 0;
    //The caller stopped waiting: a late answer is dropped.
    virtual void cancelTrackedRequest(const QString &requestId) = 0;
};

#endif // PROGRAMTRANSPORT_H
