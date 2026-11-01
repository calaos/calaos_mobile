#ifndef HARDWAREUTILS_ANDROID_H
#define HARDWAREUTILS_ANDROID_H

#include "../src/HardwareUtils.h"

//Push: the FCM registration token (from Java: persisted value at startup, then
//FirebaseMessaging.getToken() and every onNewToken()) goes through
//HardwareUtils::setDeviceToken(), and notification taps through
//HardwareUtils::handleNotificationOpened(): same policy as iOS. The push key
//(CredentialStore.pushKey()) is read at startup; admin code arrivals and taps
//go through HardwareUtils::handleAdminCodeNotified().
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


    virtual QString getDeviceName();

    //Credentials and server host live on the Java side (CredentialStore:
    //Keystore-encrypted SharedPreferences), where the push picture worker
    //can read them without Qt running: the Android counterpart of the iOS
    //shared keychain + App Group. loadAuthKeychain() migrates a pair saved
    //in QSettings by earlier versions and deletes that clear-text copy.
    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual void resetAuthKeychain();
    virtual void setConfigOption(QString key, QString value);
    //CredentialStore's session_active flag, shared with PushPictureWorker.
    virtual void setSessionActive(bool active);
    virtual bool isSessionActive();
    //CredentialStore's copy of the TLS pins, for PushPictureWorker.
    virtual void setTlsPins(const QVariantMap &pins);

    //Admin code stash written by CalaosMessagingService (CredentialStore);
    //removing an entry also cancels its notification.
    virtual void loadPushKey();
    //Sensitive ClipData; FLAG_SECURE while a code is shown.
    virtual SecretCopy copySecret(const QString &text, int seconds);
    virtual void setSecureScreen(bool on);
    virtual QList<QJsonObject> adminCodeStash();
    virtual void removeAdminCodes(const QStringList &codeIds);

private:
    //Qt's Android backend (ConnectivityManager default-network callback) feeds
    //QNetworkInformation; its changes become networkStatusChanged(), like
    //Reachability does on iOS.
    void setupNetworkMonitoring();
    int networkStatusFromQt(bool *known) const;
    void emitNetworkStatusIfChanged();

    //Last status handed out, so one network event reported through both
    //reachabilityChanged and transportMediumChanged emits only once.
    int m_lastNetworkStatus = -1;
};

#endif // HARDWAREUTILS_ANDROID_H
