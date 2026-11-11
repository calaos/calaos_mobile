#include "VersionsReport.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

namespace VersionsReport
{

//anything but a string reads as "": the daemon's rule for an unreadable field
static QString field(const QJsonObject &o, const char *key)
{
    return o.value(QLatin1String(key)).toString();
}

void setSystem(Report &report, const QJsonValue &data)
{
    if (!data.isObject())
    {
        report.hasSystem = false;
        return;
    }

    const QJsonObject o = data.toObject();
    report.hasSystem = true;
    report.failure = Failure::None;
    report.error.clear();
    report.calaosOs = field(o, "calaos_os");
    report.debian = field(o, "debian");
    report.kernel = field(o, "kernel");
    report.container = field(o, "container");
    report.packages.clear();
    const QJsonArray packages = o.value(QStringLiteral("packages")).toArray();
    for (const QJsonValue &v: packages)
    {
        const QJsonObject p = v.toObject();
        Package pkg { field(p, "name"), field(p, "version") };
        if (!pkg.name.isEmpty())
            report.packages.append(pkg);
    }
}

Report fromRelayResult(const QJsonValue &result)
{
    const QJsonObject o = result.toObject();
    Report report;
    report.server = field(o, "server");
    setSystem(report, o.value(QStringLiteral("system")));
    if (!report.hasSystem)
    {
        report.error = field(o, "error");
        report.failure = report.error == QLatin1String("daemon_too_old") ? Failure::DaemonTooOld
                                                                          : Failure::Unreachable;
    }
    return report;
}

Report fromRelayError(const QString &code, const QString &serverVersion)
{
    Report report;
    report.server = serverVersion;
    report.error = code;
    //an unknown action is refused as invalid_request by the servers before it
    report.failure = code == QLatin1String("old_server") || code == QLatin1String("invalid_request")
                         ? Failure::OldServer
                         : Failure::Unreachable;
    return report;
}

Report fromDaemon(bool ok, int httpStatus, const QJsonValue &data, const QString &serverVersion)
{
    Report report;
    report.server = serverVersion;
    if (ok && data.isObject())
    {
        setSystem(report, data);
        return report;
    }

    if (httpStatus == 404)
    {
        report.failure = Failure::DaemonTooOld;
        report.error = QStringLiteral("daemon_too_old");
    }
    else
    {
        report.failure = Failure::Unreachable;
        report.error = httpStatus == 0 ? QStringLiteral("daemon_unreachable")
                                       : QStringLiteral("http %1").arg(httpStatus);
    }
    return report;
}

QString formatText(const Report &report, const QString &clientName, const QString &clientVersion,
                   const QString &qtVersion, const QDateTime &now)
{
    QStringList lines;
    lines << QStringLiteral("Calaos versions (%1)").arg(now.toUTC().date().toString(Qt::ISODate));

    QString client = QStringLiteral("client: %1 %2").arg(clientName, clientVersion);
    if (!qtVersion.isEmpty())
        client += QStringLiteral(" (Qt %1)").arg(qtVersion);
    lines << client;

    lines << (report.server.isEmpty() ? QStringLiteral("calaos-server unavailable")
                                      : QStringLiteral("calaos-server %1").arg(report.server));

    if (!report.hasSystem)
    {
        lines << (report.error.isEmpty() ? QStringLiteral("calaos-container unreachable")
                                         : QStringLiteral("calaos-container unreachable (%1)").arg(report.error));
        return lines.join(QLatin1Char('\n'));
    }

    if (!report.container.isEmpty())
        lines << QStringLiteral("calaos-container %1").arg(report.container);

    QStringList os;
    if (!report.calaosOs.isEmpty())
        os << QStringLiteral("calaos-os %1").arg(report.calaosOs);
    if (!report.debian.isEmpty())
        os << QStringLiteral("debian %1").arg(report.debian);
    if (!report.kernel.isEmpty())
        os << QStringLiteral("kernel %1").arg(report.kernel);
    if (!os.isEmpty())
        lines << os.join(QStringLiteral(" · "));

    for (const Package &p: report.packages)
        lines << (p.version.isEmpty() ? p.name : QStringLiteral("%1 %2").arg(p.name, p.version));

    return lines.join(QLatin1Char('\n'));
}

}
