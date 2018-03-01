#ifndef HARDWAREUTILS_H
#define HARDWAREUTILS_H

#include <QtCore>

class QQmlApplicationEngine;

class HardwareUtils: public QObject
{
    Q_OBJECT

protected:
    HardwareUtils(QObject *parent = 0);

    bool appIsBackground = false;

    bool startedWithOpt = false;
    bool startedWithNotif = false;
    QString notifUuid;
    QQmlApplicationEngine *qmlEngine = nullptr;

    //Device unique id used for push notifications
    QString deviceToken;

    //state of calaos connection (logged in/out)
    bool calaosConnected = false;

public:
    static HardwareUtils *Instance(QObject *parent = NULL);

    virtual ~HardwareUtils();

    virtual void platformInit(QQmlApplicationEngine *e);

    virtual void showAlertMessage(QString title, QString message, QString buttontext);

    enum NetworkStatus
    {
        NotConnected = 0,
        ConnectedWiFi = 2,
        ConnectedWWAN = 1
    };
    virtual int getNetworkStatus();

    virtual void showNetworkActivity(bool en);

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual QString getServerHost();
    virtual void resetAuthKeychain();

    virtual void setConfigOption(QString key, QString value);
    virtual QString getConfigOption(QString key);

    virtual void inputTextDialog(const QString &title, const QString &message);

    //This is set by platform code if app was started with options
    //It is used by iOS when started with a QuickAction
    bool hasStartedWithOption() { return startedWithOpt; }
    virtual QString getStartOption(const QString &key) { Q_UNUSED(key); return QString(); }

    //Used when user clicked on a notification
    //It should open the app and load the notif
    bool hasStartedWithNotif() { return startedWithNotif; }
    QString getNotifUuid() { return notifUuid; }

    virtual void setQuickLinks(QVariantList quicklinks) { Q_UNUSED(quicklinks); }

    QString getDeviceToken() { return deviceToken; }

    //let the HardwareUtils class know about the connection state
    void updateCalaosConnectState(bool connected) { calaosConnected = connected; }
    bool isCalaosConnected() { return calaosConnected; }

    //This is used by private classes
    void emitNetworkStatusChanged();
    void emitApplicationActiveChanged(bool active);
    void emitDialogTextValid(const QString &s);
    void emitDialogCancel();
    void emitCalaosServerDetected();
    void emitPushNotifReceived(const QString &uuid);

signals:
    void networkStatusChanged();
    void applicationWillResignActive();
    void applicationBecomeActive();

    void dialogCanceled();
    void dialogTextValid(const QString &text);

    //emited when a calaos_server has been detected on the LAN
    void calaosServerDetected();

    //emited when in foreground and a push notif has been received
    void pushNotifReceived(const QString &uuid);
};

#endif // HARDWAREUTILS_H
