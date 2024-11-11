#include "Machine.h"
#include <QHostAddress>

NetworkInfo::NetworkInfo(QObject *parent):
    QObject(parent)
{
    update_isLoopback(false);
}

void NetworkInfo::setIPv4CIDR(QString cidr)
{
    QStringList parts = cidr.split('/');
    if (parts.size() != 2) {
        qWarning() << "invalid CIDR format: " << cidr;
        return;
    }

    QString ipAddress = parts[0];
    int prefixLength = parts[1].toInt();
    if (prefixLength < 0 || prefixLength > 32) {
        qWarning() << "invalid prefix length: " << prefixLength;
        return;
    }

    quint32 netmaskInt = ~((1 << (32 - prefixLength)) - 1);
    QHostAddress netmask(netmaskInt);

    update_ipv4(ipAddress);
    update_netmask(netmask.toString());
}

QJsonObject NetworkInfo::toJson() const
{
    QJsonObject obj;
    obj["name"] = get_netinterface();
    obj["ipv4"] = QString("%1/%2").arg(get_ipv4()).arg(netmaskToCidr(get_netmask()));
    obj["gateway"] = get_gateway();
    obj["dhcp"] = get_isDHCP();

    QJsonArray dnsServers;
    for (const QString &dns: get_dnsServers().split(',', Qt::SkipEmptyParts))
    {
        dnsServers.append(dns.trimmed());
    }

    obj["dns_servers"] = dnsServers;

    QJsonArray searchDomains;
    for (const QString &domain: get_searchDomains().split(',', Qt::SkipEmptyParts))
    {
        searchDomains.append(domain.trimmed());
    }

    obj["search_domains"] = searchDomains;

    return obj;
}

int NetworkInfo::netmaskToCidr(const QString &netmask) const
{
    QHostAddress address(netmask);
    quint32 mask = address.toIPv4Address();

    int cidr = 0;
    while (mask & (1 << (31 - cidr)))
    {
        cidr++;
    }
    return cidr;
}
