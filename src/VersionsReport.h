#ifndef VERSIONSREPORT_H
#define VERSIONSREPORT_H

#include <QDateTime>
#include <QJsonValue>
#include <QList>
#include <QString>

//Versions of the box: calaos_server's, and the system's as calaos-container
//reports it (GET /api/v2/system/versions). The kiosk asks the daemon directly,
//the phone through the "versions" admin action of calaos_server.
namespace VersionsReport
{

struct Package
{
    QString name;
    QString version;
};

enum class Failure
{
    None,
    OldServer, //calaos_server does not know the "versions" action
    DaemonTooOld, //calaos-container has no versions route
    Unreachable,
    Unauthorized, //the kiosk token was refused, even after a reload
};

struct Report
{
    QString server;
    bool hasSystem = false;
    QString calaosOs;
    QString debian;
    QString kernel;
    QString container;
    QList<Package> packages;
    Failure failure = Failure::None;
    //short reason when hasSystem is false: the server's message or an error code
    QString error;
};

//The data of the daemon's answer (calaos-container docs/API.md)
void setSystem(Report &report, const QJsonValue &data);

//The result of a successful "versions" admin action
Report fromRelayResult(const QJsonValue &result);
//The admin action failed: only the server version from get_server_info is known
Report fromRelayError(const QString &code, const QString &serverVersion);

//The daemon's answer on the kiosk; httpStatus 0 is a transport error
Report fromDaemon(bool ok, int httpStatus, const QJsonValue &data, const QString &serverVersion);

//The text of the Copy button, one line per component, as the web app writes it
QString formatText(const Report &report, const QString &clientName, const QString &clientVersion,
                   const QString &qtVersion, const QDateTime &now);

}

#endif // VERSIONSREPORT_H
