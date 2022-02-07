#ifndef UPTIME_H
#define UPTIME_H

#include <QtCore>
#include "qqmlhelpers.h"

class NetworkInfo: public QObject
{
    Q_OBJECT
    QML_READONLY_PROPERTY(QString, netinterface)
    QML_READONLY_PROPERTY(QString, ipv4)
    QML_READONLY_PROPERTY(QString, ipv6)
    QML_READONLY_PROPERTY(QString, mac)
    QML_READONLY_PROPERTY(bool, isLoopback)
public:
    NetworkInfo(QObject *parent = nullptr): QObject(parent) {}
};

class Machine
{
public:
    static quint32 getMachineUptime();
    static QString getHostname();
    static QList<NetworkInfo *> getNetworkInfo();
    static int getMemoryUsage();
    static int getCpuUsage();
    static bool isBootReadOnly();
};

#endif // UPTIME_H
