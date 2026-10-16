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

    //The FCM registration token (from Java: persisted value at startup, then
    //FirebaseMessaging.getToken() and every onNewToken()) goes through
    //HardwareUtils::setDeviceToken(), and notification taps through
    //HardwareUtils::handleNotificationOpened(): same policy as iOS.

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
