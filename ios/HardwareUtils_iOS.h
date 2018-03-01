#ifndef HARDWAREUTILS_IOS_H
#define HARDWAREUTILS_IOS_H

#include "../src/HardwareUtils.h"

class HardwareUtils_iOS: public HardwareUtils
{
protected:
    friend class HardwareUtils;
    HardwareUtils_iOS(QObject *parent = 0);

    QString ioStartShortcut;

public:
    virtual ~HardwareUtils_iOS();

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual void showNetworkActivity(bool en);
    
    virtual void setConfigOption(QString key, QString value);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual void resetAuthKeychain();

    virtual void inputTextDialog(const QString &title, const QString &message);

    virtual QString getStartOption(const QString &key);
    virtual void setQuickLinks(QVariantList quicklinks);

    void handleApplicationDidFinishLaunching(void *notif);
    void handlePerformActionForShortcutItem(void *shortcut);
    void handleRegisterNotif(void *data);
    void handleRegisterNotifFail(void *err);
    void handleStartedWithPushNotif(const QString &uuid);
};

#endif // HARDWAREUTILS_IOS_H
