#include "VersionsModel.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QVariantMap>

using namespace VersionsReport;

VersionsModel::VersionsModel(const QString &clientName, const QString &clientVersion, QObject *parent):
    QObject(parent)
{
    update_clientName(clientName);
    update_clientVersion(clientVersion);
    update_qtVersion(QString::fromLatin1(qVersion()));
}

void VersionsModel::refresh()
{
    if (!m_fetcher)
        return;

    //only the last request may fill the page: an older answer arrives late
    const int seq = ++m_seq;
    update_loading(true);
    QPointer<VersionsModel> self(this);
    m_fetcher([self, seq](const Report &report)
    {
        if (!self || seq != self->m_seq)
            return;
        self->apply(report);
    });
}

void VersionsModel::apply(const Report &report)
{
    m_report = report;

    QVariantList packages;
    for (const Package &p: report.packages)
        packages.append(QVariantMap{{ QStringLiteral("name"), p.name },
                                    { QStringLiteral("version"), p.version }});

    update_serverVersion(report.server);
    update_systemAvailable(report.hasSystem);
    update_containerVersion(report.container);
    update_calaosOs(report.calaosOs);
    update_debian(report.debian);
    update_kernel(report.kernel);
    update_packages(packages);

    switch (report.failure)
    {
    case Failure::None:
        update_failure(QString());
        update_message(QString());
        break;
    case Failure::OldServer:
        update_failure(QStringLiteral("old_server"));
        update_message(tr("This server cannot give the box versions: update calaos-server."));
        break;
    case Failure::DaemonTooOld:
        update_failure(QStringLiteral("daemon_too_old"));
        update_message(tr("The box system is too old to give its versions: update calaos-container."));
        break;
    case Failure::Unreachable:
        update_failure(QStringLiteral("unreachable"));
        update_message(tr("The box system did not answer: only the server and client versions are shown."));
        break;
    }

    update_loaded(true);
    update_loading(false);
}

QString VersionsModel::reportTextAt(const QDateTime &now) const
{
    return formatText(m_report, m_clientName, m_clientVersion, m_qtVersion, now);
}

QString VersionsModel::reportText() const
{
    return reportTextAt(QDateTime::currentDateTimeUtc());
}

void VersionsModel::copyToClipboard() const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(reportText());
}
