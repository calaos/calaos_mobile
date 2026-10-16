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

    //Credentials and server host live on the Java side (CredentialStore:
    //Keystore-encrypted SharedPreferences), where the push picture worker
    //can read them without Qt running: the Android counterpart of the iOS
    //shared keychain + App Group. loadAuthKeychain() migrates a pair saved
    //in QSettings by earlier versions and deletes that clear-text copy.
    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual void resetAuthKeychain();
    virtual void setConfigOption(QString key, QString value);

    //The FCM registration token (from Java: persisted value at startup, then
    //FirebaseMessaging.getToken() and every onNewToken()) goes through
    //HardwareUtils::setDeviceToken(), and notification taps through
    //HardwareUtils::handleNotificationOpened(): same policy as iOS.
};

#endif // HARDWAREUTILS_ANDROID_H
