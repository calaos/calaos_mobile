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
    //Also forgets the session state (isSessionActive() is true again).
    virtual void resetAuthKeychain();

    virtual void setConfigOption(QString key, QString value);
    virtual QString getConfigOption(QString key);

    //Persistent "not logged out by the user" state: true once home is
    //loaded, false on an explicit user logout only (Application::userLogout()),
    //never on a lost connection, a reconnection or the iOS resign-active
    //pause. It survives a restart: while false, no automatic login runs
    //(LaunchLogin, FIX-11), and platform code running without the app (Android
    //push picture worker, iOS notification service extension) does not use
    //the stored credentials. True when never set (fresh install, update) and
    //again after resetAuthKeychain().
    //Base implementation (desktop, tests): QSettings.
    virtual void setSessionActive(bool active);
    virtual bool isSessionActive();

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

    //Puts back a deferred uuid that was taken but not opened yet because the
    //session dropped first (see DeferredNotifGate). A newer tap already
    //waiting wins.
    void restorePendingNotif(const QString &uuid);

    //The user logged out on purpose (Application::userLogout(), and
    //Application::resetAllData() which also wipes host and credentials): what was
    //asked of the previous session (a notification tapped but not opened
    //yet, an iOS Quick Action) must not fire, possibly stale or on another
    //home, at the next manual login. Not called on a lost connection, the
    //login cancel button or the iOS resign-active pause.
    void forgetPendingUserActions();

    virtual void setQuickLinks(QVariantList quicklinks) { Q_UNUSED(quicklinks); }

    QString getDeviceToken() { return deviceToken; }

    //Platform code reports the current push token here (APNs / FCM, at
    //startup and on every rotation). deviceTokenChanged() is emitted when the
    //value actually changes, so the server can be told about it.
    void setDeviceToken(const QString &token);

    //No usable token any more (APNs registration failed). Also lets the unit
    //tests start a case from "no token yet" on the singleton.
    void clearDeviceToken();

    //A push token is a per-device credential: never log it in full. Returns
    //the first characters and the length, for diagnostics.
    static QString describeToken(const QString &token);

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

    //emited when the push token changed (see setDeviceToken())
    void deviceTokenChanged();
};

//When Application opens a deferred notification tap (FIX-3).
//
//At every login, homeLoaded() takes the deferred uuid from HardwareUtils and
//opens it a little later, once the home page is shown. Each login starts a
//new generation: a timer armed by an earlier login is void, and a uuid taken
//but not opened yet when the session drops is handed back, so the next login
//opens it instead. The tap is therefore opened exactly once, by the session
//that is up when it is opened.
//
//Header-only, no timer and no QObject, so that tests/tst_pushnotif can drive
//it; Application only owns the QTimer.
class DeferredNotifGate
{
public:
    //Home is loaded. uuid is what HardwareUtils::takePendingNotif() returned.
    //Returns the ticket to hand back to fire() when the delay expires, 0 when
    //there is nothing to open.
    quint64 loggedIn(const QString &uuid)
    {
        ++m_generation;
        if (!uuid.isEmpty())
            m_inFlight = uuid;
        return m_inFlight.isEmpty() ? 0 : m_generation;
    }

    //The delay armed with ticket expired. Returns the uuid to open now, or an
    //empty string when the ticket belongs to an earlier login.
    QString fire(quint64 ticket)
    {
        if (ticket == 0 || ticket != m_generation)
            return QString();
        QString uuid;
        uuid.swap(m_inFlight);
        return uuid;
    }

    //The session is down. Every armed delay becomes stale; returns the uuid
    //that was taken but not opened yet, for HardwareUtils::restorePendingNotif().
    QString loggedOut()
    {
        ++m_generation;
        QString uuid;
        uuid.swap(m_inFlight);
        return uuid;
    }

private:
    quint64 m_generation = 0;
    QString m_inFlight;
};

#endif // HARDWAREUTILS_H
