#include "Machine.h"
#include <QDebug>

#if defined(Q_OS_WIN32)

#include <qt_windows.h>
#include <tchar.h>
#include <iphlpapi.h>
#include <ipifcons.h>
#include <pdh.h>

quint32 Machine::getMachineUptime()
{
    qDebug() << GetTickCount64();
    return GetTickCount64() / 1000;
}

QString Machine::getHostname()
{
    TCHAR buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;

    if (GetComputerName(buf, &sz))
        return QString::fromWCharArray(buf, sz);

    return QStringLiteral("Unknown");
}

#define ALLOCATE_FROM_PROCESS_HEAP(bytes)		::HeapAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, bytes )
#define DEALLOCATE_FROM_PROCESS_HEAP(ptr)		if( ptr ) ::HeapFree( ::GetProcessHeap(), 0, ptr )
#define REALLOC_FROM_PROCESS_HEAP(ptr, bytes)	::HeapReAlloc( ::GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, bytes )

QList<NetworkInfo *> Machine::getNetworkInfo()
{
    QList<NetworkInfo *> netList;

    IP_ADAPTER_INFO *pAdptInfo = NULL;
    IP_ADAPTER_INFO *pNextAd = NULL;
    ULONG ulLen = 0;
    ULONG erradapt;

    erradapt = ::GetAdaptersInfo(pAdptInfo, &ulLen);
    if (erradapt == ERROR_BUFFER_OVERFLOW)
    {
        pAdptInfo = (IP_ADAPTER_INFO *)ALLOCATE_FROM_PROCESS_HEAP(ulLen);
        erradapt = ::GetAdaptersInfo(pAdptInfo, &ulLen);
    }

    pNextAd = pAdptInfo;
    while (pNextAd)
    {
        NetworkInfo *net = new NetworkInfo();
        net->update_netinterface(QString::fromUtf8(pNextAd->Description));
        if (pNextAd->CurrentIpAddress)
            net->update_ipv4(QString::fromUtf8(pNextAd->CurrentIpAddress->IpAddress.String));
        else
        {
            IP_ADDR_STRING *pNext = &(pNextAd->IpAddressList);
            if (pNext)
                net->update_ipv4(QString::fromUtf8(pNext->IpAddress.String));
            else
                net->update_ipv4("0.0.0.0");
        }
        net->update_isLoopback(pNextAd->Type == MIB_IF_TYPE_LOOPBACK);

        netList.append(net);
        pNextAd = pNextAd->Next;
    }

    DEALLOCATE_FROM_PROCESS_HEAP(pAdptInfo);

    return netList;
}

int Machine::getCpuUsage()
{
    FILETIME               ft_sys_idle;
    FILETIME               ft_sys_kernel;
    FILETIME               ft_sys_user;

    ULARGE_INTEGER         ul_sys_idle;
    ULARGE_INTEGER         ul_sys_kernel;
    ULARGE_INTEGER         ul_sys_user;

    static ULARGE_INTEGER    ul_sys_idle_old;
    static ULARGE_INTEGER  ul_sys_kernel_old;
    static ULARGE_INTEGER  ul_sys_user_old;

    CHAR  usage = 0;
    // we cannot directly use GetSystemTimes on C language
    /* add this line :: pfnGetSystemTimes */
    GetSystemTimes(&ft_sys_idle,    /* System idle time */
                   &ft_sys_kernel,  /* system kernel time */
                   &ft_sys_user);   /* System user time */

    CopyMemory(&ul_sys_idle  , &ft_sys_idle  , sizeof(FILETIME)); // Could been optimized away...
    CopyMemory(&ul_sys_kernel, &ft_sys_kernel, sizeof(FILETIME)); // Could been optimized away...
    CopyMemory(&ul_sys_user  , &ft_sys_user  , sizeof(FILETIME)); // Could been optimized away...

    usage  =
            (((((ul_sys_kernel.QuadPart - ul_sys_kernel_old.QuadPart) +
                (ul_sys_user.QuadPart - ul_sys_user_old.QuadPart)) -
               (ul_sys_idle.QuadPart-ul_sys_idle_old.QuadPart)) * 100) /
             ((ul_sys_kernel.QuadPart - ul_sys_kernel_old.QuadPart) +
              (ul_sys_user.QuadPart - ul_sys_user_old.QuadPart)));

    ul_sys_idle_old.QuadPart = ul_sys_idle.QuadPart;
    ul_sys_user_old.QuadPart = ul_sys_user.QuadPart;
    ul_sys_kernel_old.QuadPart = ul_sys_kernel.QuadPart;

    return usage;
}

int Machine::getMemoryUsage()
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    return (memInfo.ullTotalPhys - memInfo.ullAvailPhys) * 100.0 / memInfo.ullTotalPhys;
}

#endif /* Q_OS_WIN32 */


#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#if HAVE_STROPTS_H
#include <stropts.h>
#endif

quint32 Machine::getMachineUptime()
{
    struct sysinfo info;
    if (sysinfo(&info) != 0)
        return -1;
    return info.uptime;
}

static void _getLocalInfo(QString intf, QString &ipv4, QString &mac)
{
    struct ifreq ifr;

    int skfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (skfd < 0)
    {
        qDebug() << "can't open socket! " << strerror(errno);
        return;
    }

    strncpy(ifr.ifr_name, intf.toLocal8Bit().data(), IFNAMSIZ);

    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        close(skfd);
        return;
    }

    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0)
    {
        close(skfd);
        return;
    }

    ipv4 = inet_ntoa(*(struct in_addr *) &(((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr));

    if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        close(skfd);
        return;
    }

    mac = QByteArray(ifr.ifr_hwaddr.sa_data, 6);

    close(skfd);
}

#define SYSCLASSNET     "/sys/class/net"
QList<NetworkInfo *> Machine::getNetworkInfo()
{
    QList<NetworkInfo *> netList;

    QDir d(SYSCLASSNET);
    QStringList intf = d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);

    foreach(const QString &inf, intf)
    {
        NetworkInfo *net = new NetworkInfo();
        net->update_netinterface(inf);
        QString ipv4, mac;
        _getLocalInfo(inf, ipv4, mac);
        net->update_ipv4(ipv4);
        net->update_mac(mac);
        net->update_isLoopback(inf == "lo");
        netList.append(net);
    }

    return netList;
}

static quint32 _lastTotalJiffies = 0;
static quint32 _lastWorkJiffies = 0;

#define CPUSTAT     "/proc/stat"
int Machine::getCpuUsage()
{
    QFile f(CPUSTAT);
    if (!f.open(QFile::ReadOnly))
        return 0;

    QString data = f.readAll();

    QStringList statList;
    foreach (const QString &l, data.split('\n', Qt::SkipEmptyParts))
    {
        QString line = l.trimmed();
        if (line.startsWith("cpu "))
        {
            statList = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            break;
        }
    }

    if (statList.count() < 8)
        return 0;

    quint32 totalJiffies = 0;
    quint32 workJiffies = 0;

    for (int i = 1;i < statList.count();i++)
    {
        totalJiffies += statList.at(i).toInt();
        if (i < 4)
            workJiffies += statList.at(i).toInt();
    }

    if (_lastTotalJiffies == 0)
    {
        _lastTotalJiffies = totalJiffies;
        _lastWorkJiffies = workJiffies;
        return 0;
    }

    quint32 work = workJiffies - _lastWorkJiffies;
    quint32 total = totalJiffies - _lastTotalJiffies;

    return work * 100 / total;
}

int Machine::getMemoryUsage()
{
    struct sysinfo info;
    if (sysinfo(&info) != 0)
        return -1;
    return (info.totalram - info.freeram) * 100.0 / info.totalram;
}

#endif /* Q_OS_LINUX */


#if defined(Q_OS_OSX)
#include <time.h>
#include <errno.h>
#include <sys/sysctl.h>

quint32 Machine::getMachineUptime()
{
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) < 0)
        return 0;
    return time(NULL) - boottime.tv_sec;
}

QList<NetworkInfo *> Machine::getNetworkInfo()
{
    QList<NetworkInfo *> l;
    return l;
}

int Machine::getMemoryUsage()
{
    return 0;
}

int Machine::getCpuUsage()
{
    return 0;
}

#endif /* Q_OS_OSX */


#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
quint32 Machine::getMachineUptime()
{
    return 0;
}

QList<NetworkInfo *> Machine::getNetworkInfo()
{
    QList<NetworkInfo *> l;
    return l;
}

int Machine::getMemoryUsage()
{
    return 0;
}

int Machine::getCpuUsage()
{
    return 0;
}
#endif


#ifndef Q_OS_WIN32
QString Machine::getHostname()
{
#ifdef  _POSIX_HOSTNAME_MAX
    char hostname[_POSIX_HOSTNAME_MAX];
    gethostname(hostname, _POSIX_HOSTNAME_MAX);
#elif defined HOSTNAME_MAX
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
#elif defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    char hostname[128];
    gethostname(hostname, 128);
#endif
    return QString("calaos");
}
#endif
