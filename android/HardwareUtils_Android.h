#ifndef HARDWAREUTILS_ANDROID_H
#define HARDWAREUTILS_ANDROID_H

#include "../src/HardwareUtils.h"

class HardwareUtilsAndroid: public HardwareUtils
{
private:
    friend class HardwareUtils;
    HardwareUtilsAndroid(QObject *parent = 0);

public:
    virtual ~HardwareUtilsAndroid();

    virtual void platformInit(QQmlApplicationEngine *e);

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual void inputTextDialog(const QString &title, const QString &message);

    //FCM registration token (from Java: persisted value at startup, then
    //FirebaseMessaging.getToken() and every onNewToken()).
    void setDeviceToken(QString t);

    //The user tapped a push notification carrying a Calaos event uuid while
    //the activity was alive. Same policy as iOS: open it now if logged in,
    //otherwise keep it for Application to open once home is loaded.
    void handleNotificationOpened(const QString &uuid);
};

#endif // HARDWAREUTILS_ANDROID_H
