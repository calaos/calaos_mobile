#ifndef HARDWAREUTILSDESK_H
#define HARDWAREUTILSDESK_H

#include "HardwareUtils.h"
#include <QTimer>
#include <QUdpSocket>

class HardwareUtilsDesktop: public HardwareUtils
{
    Q_OBJECT

public:
    virtual ~HardwareUtilsDesktop();

    void platformInit(QQmlApplicationEngine *e);

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    virtual void showNetworkActivity(bool en);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual QString getServerHost();

    virtual void setConfigOption(QString key, QString value);
    virtual QString getConfigOption(QString key);

    QString getConfigFile(QString configType);
    QString getCacheFile(QString cacheFile);

private slots:
    void readPendingDatagrams(void);
    void calaosDiscover();

protected:
    friend class HardwareUtils;
    HardwareUtilsDesktop(QObject *parent = 0);

    QString configDir, cacheDir, configBase, cacheBase;
    void initConfigOptions(QString configdir, QString cachedir);
    QHash<QString, QString> getAllOptions();

    //for calaos_server detection
    QUdpSocket *udpSocket = nullptr;
    QTimer *timer = nullptr;
    QString calaosServerHost;
};

#endif // HARDWAREUTILSDESK_H
