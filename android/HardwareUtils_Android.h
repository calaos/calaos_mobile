#ifndef HARDWAREUTILS_ANDROID_H
#define HARDWAREUTILS_ANDROID_H

#include "../src/HardwareUtils.h"

class FbListener;

class HardwareUtilsAndroid: public HardwareUtils
{
private:
    friend class HardwareUtils;
    HardwareUtilsAndroid(QObject *parent = 0);

    FbListener *fcmListener = nullptr;

public:
    virtual ~HardwareUtilsAndroid();

    static HardwareUtilsAndroid *Instance(QObject *parent = NULL);

    virtual void platformInit(QQmlApplicationEngine *e);

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    virtual void loadAuthKeychain(QString &email, QString &pass);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);

    virtual void setConfigOption(QString key, QString value);

    virtual void resetAuthKeychain();

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual void inputTextDialog(const QString &title, const QString &message);

    FbListener *getFbListener() { return fcmListener; }

    void setDeviceToken(QString t);
};

#endif // HARDWAREUTILS_ANDROID_H
