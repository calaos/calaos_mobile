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
    QQmlApplicationEngine *qmlEngine = nullptr;

    //Device unique id used for push notifications
    QString deviceToken;

    //state of calaos connection (logged in/out)
    bool calaosConnected = false;

private:
    //Event uuid of a notification tapped while not logged in
    QString pendingNotifUuid;

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

    virtual void saveAuthKeychain(const QString &email, const QString &pass);
    virtual void loadAuthKeychain(QString &email, QString &pass);
    virtual QString getServerHost();
    virtual void resetAuthKeychain();

    virtual void setConfigOption(QString key, QString value);
    virtual QString getConfigOption(QString key);

    //Called from qml/mobile/main.qml: the QML layer decides when a value has
    //to be typed, the platform decides how. The answer comes back through the
    //dialogTextValid()/dialogCanceled() signals below.
    Q_INVOKABLE virtual void inputTextDialog(const QString &title, const QString &message);

    //This is set by platform code if app was started with options
    //It is used by iOS when started with a QuickAction
    bool hasStartedWithOption() { return startedWithOpt; }
    virtual QString getStartOption(const QString &key) { Q_UNUSED(key); return QString(); }
    void resetStartedWithOption() { startedWithOpt = false; }

    //The user tapped a push notification carrying a Calaos event uuid (cold
    //start or app already running). Logged in: pushNotifReceived() is emitted
    //right away. Otherwise the uuid is kept, the latest tap wins, until
    //Application takes it once home is loaded again (takePendingNotif()).
    void handleNotificationOpened(const QString &uuid);

    //Returns the deferred notification uuid, if any, and forgets it: a
    //deferred tap is opened exactly once.
    QString takePendingNotif();
    bool hasPendingNotif() const { return !pendingNotifUuid.isEmpty(); }

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
