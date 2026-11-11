#ifndef VERSIONSMODEL_H
#define VERSIONSMODEL_H

#include <functional>
#include <QDateTime>
#include <QObject>
#include <QVariantList>
#include "qqmlhelpers.h"
#include "VersionsReport.h"

//The versions page, kiosk and phone alike. Where the versions come from is
//the fetcher's business: the daemon on the kiosk, the admin relay on the phone.
class VersionsModel: public QObject
{
    Q_OBJECT

    QML_READONLY_PROPERTY(bool, loading)
    //a report arrived, the values below are its own
    QML_READONLY_PROPERTY(bool, loaded)

    QML_READONLY_PROPERTY(QString, clientName)
    QML_READONLY_PROPERTY(QString, clientVersion)
    QML_READONLY_PROPERTY(QString, qtVersion)

    QML_READONLY_PROPERTY(QString, serverVersion)
    //calaos-container answered: the system values are the box's
    QML_READONLY_PROPERTY(bool, systemAvailable)
    QML_READONLY_PROPERTY(QString, containerVersion)
    QML_READONLY_PROPERTY(QString, calaosOs)
    QML_READONLY_PROPERTY(QString, debian)
    QML_READONLY_PROPERTY(QString, kernel)
    //[{ name, version }], sorted by the daemon
    QML_READONLY_PROPERTY(QVariantList, packages)

    //why the system versions are missing: "", "old_server", "daemon_too_old", "unreachable"
    QML_READONLY_PROPERTY(QString, failure)
    QML_READONLY_PROPERTY(QString, message)

public:
    using Done = std::function<void(const VersionsReport::Report &report)>;
    using Fetcher = std::function<void(Done done)>;

    VersionsModel(const QString &clientName, const QString &clientVersion, QObject *parent = nullptr);

    void setFetcher(Fetcher fetcher) { m_fetcher = std::move(fetcher); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString reportText() const;
    Q_INVOKABLE void copyToClipboard() const;

    QString reportTextAt(const QDateTime &now) const;

private:
    void apply(const VersionsReport::Report &report);

    Fetcher m_fetcher;
    VersionsReport::Report m_report;
    int m_seq = 0;
};

#endif // VERSIONSMODEL_H
