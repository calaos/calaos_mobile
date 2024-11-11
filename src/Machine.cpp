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
