#ifndef HARDWAREUTILS_H
#define HARDWAREUTILS_H

#include <QtCore>

class QQmlApplicationEngine;

class HardwareUtils: public QObject
{
    Q_OBJECT

    //For the settings screens: the name register_push sends, see
    //effectiveDeviceName().
    Q_PROPERTY(QString deviceName READ effectiveDeviceName NOTIFY deviceNameChanged)
    Q_PROPERTY(int deviceNameMaxLength READ deviceNameMaxLength CONSTANT)

protected:
    HardwareUtils(QObject *parent = 0);

    bool appIsBackground = false;

    bool startedWithOpt = false;
    QQmlApplicationEngine *qmlEngine = nullptr;

    //Device unique id used for push notifications
    QString deviceToken;

    //state of calaos connection (logged in/out)
    bool calaosConnected = false;

    //Shows the platform dialog of requestNativePassword(); false if it could
    //not. The answer comes back through emitNativePassword*().
    virtual bool showNativePasswordEntry(const QString &purpose, const QVariantMap &options)
    {
        Q_UNUSED(purpose) Q_UNUSED(options)
        return false;
    }

private:
    //Event uuid of a notification tapped while not logged in
    QString pendingNotifUuid;
    //The request whose dialog is on screen, empty when none.
    QString pendingPasswordPurpose;
    QString inlineEntryPurpose;

    //Admin code notified before the code sheet could read the stash
    QString pendingAdminCodeId;

    //See pushKey()
    QByteArray m_pushKey;

    //Base stash (desktop, tests): in memory, keyed by code_id
    QMap<QString, QJsonObject> m_adminCodeStash;

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

    //Copy of the TLS pins (host:port -> SPKI SHA-256 hex, see TlsPinning.h)
    //for the code that downloads push pictures without the app (Android
    //worker, iOS notification extension). Not a secret. Base: nothing to do.
    virtual void setTlsPins(const QVariantMap &pins) { Q_UNUSED(pins); }

    //Called from qml/mobile/main.qml: the QML layer decides when a value has
    //to be typed, the platform decides how. The answer comes back through the
    //dialogTextValid()/dialogCanceled() signals below.
    Q_INVOKABLE virtual void inputTextDialog(const QString &title, const QString &message);

    //Native credential entry: a platform dialog with a real username and
    //password field, the only ones the password manager (iCloud Keychain,
    //1Password, Android Autofill) can fill; Qt's own text fields cannot.
    //options: title, message, user (prefilled), userLabel, passwordLabel,
    //confirm, cancel. Exactly one nativePasswordEntered() or
    //nativePasswordCanceled() per accepted request; false (nothing shown,
    //nothing emitted) when the platform has none, or one is already open.
    //Nothing is stored or logged on the way.
    Q_INVOKABLE virtual bool hasNativePasswordEntry() const { return false; }
    Q_INVOKABLE bool requestNativePassword(const QString &purpose, const QVariantMap &options);
    //Native text fields laid over the QML ones, typed in place; where the
    //platform has them, a field that asks for one (CalaosTextField
    //inlineContentType: "username" or "password") needs no dialog. item is
    //the CalaosTextField, purpose as for requestNativePassword().
    Q_INVOKABLE virtual bool hasInlineTextFields() const { return false; }
    Q_INVOKABLE virtual void bindInlineTextField(QObject *item, const QString &purpose, const QString &contentType)
    { Q_UNUSED(item) Q_UNUSED(purpose) Q_UNUSED(contentType) }
    Q_INVOKABLE virtual void endInlineEditing() {}
    //The purpose of the native entry on screen (dialog or inline field being
    //edited), empty when none.
    QString pendingNativePasswordPurpose() const
    { return pendingPasswordPurpose.isEmpty() ? inlineEntryPurpose : pendingPasswordPurpose; }

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

    //Name of this device as its owner knows it (sent with register_push so
    //the admin can tell the phones apart). Base implementation: host name.
    virtual QString getDeviceName();

    //Since iOS 16 every iPhone reports "iPhone", so the user can name the
    //device in the settings. The name actually sent: that override when set,
    //else getDeviceName(), always through sanitizeDeviceName().
    QString effectiveDeviceName();
    //Stored sanitized (config option "device_name"). Empty, or the platform
    //name itself, means no override: the platform name is followed again.
    //deviceNameChanged() is emitted when effectiveDeviceName() changes.
    Q_INVOKABLE void setDeviceNameOverride(const QString &name);
    QString deviceNameOverride();

    //In UTF-16 code units, what a QML TextField's maximumLength counts.
    static constexpr int DeviceNameMaxLength = 64;
    static int deviceNameMaxLength() { return DeviceNameMaxLength; }
    //Control characters and line breaks become spaces, bidi controls and
    //invisible spaces are dropped, whitespace runs collapse to one space,
    //edges are trimmed, and the result is cut to DeviceNameMaxLength on a
    //grapheme boundary (never inside a surrogate pair or an emoji sequence).
    static QString sanitizeDeviceName(const QString &name);

    //A push token is a per-device credential: never log it in full. Returns
    //the first characters and the length, for diagnostics.
    static QString describeToken(const QString &token);

    //Key calaos_server seals the admin codes pushed to this device with
    //(register_push "push_key", see AdminCode.h): 32 bytes from the platform
    //CSPRNG, created once and kept in the platform key store (Keychain,
    //Keystore), never in QSettings. Empty when the platform has none (desktop)
    //or its store failed: the server then pushes the legacy text.
    QByteArray pushKey() const { return m_pushKey; }
    //Platform code reports the key here; pushKeyChanged() on a change only.
    void setPushKey(const QByteArray &key);
    //Reads the key from the platform store (created there the first time it
    //is really absent) and reports it. Called again while the key is empty:
    //the store can be briefly unavailable (iOS before the first unlock,
    //Keystore error), which must never replace an existing key.
    virtual void loadPushKey() {}

    //Admin code payloads stashed at reception by the platform (Android
    //messaging service, iOS notification extension or app delegate), exactly
    //as received plus the reception time; no cryptography there. One JSON
    //object per code, see AdminCode::Entry. Base: in memory.
    virtual QList<QJsonObject> adminCodeStash();
    //Adds an entry unless one with the same code_id is stashed already.
    virtual void stashAdminCode(const QJsonObject &entry);
    //Consumed, expired or rejected codes. Also withdraws their notification
    //where the platform can.
    virtual void removeAdminCodes(const QStringList &codeIds);

    enum class SecretCopy
    {
        NotHandled,       //caller puts it on QClipboard and clears it later
        Copied,           //caller clears it later (QClipboard reads it back)
        CopiedWithExpiry, //the platform drops it after the given time itself
    };
    //Puts a secret on the clipboard, marked sensitive where the platform
    //can (no preview), for at most seconds.
    virtual SecretCopy copySecret(const QString &text, int seconds) { Q_UNUSED(text); Q_UNUSED(seconds); return SecretCopy::NotHandled; }

    //Keeps a secret on screen out of screenshots and the app switcher where
    //the platform can (Android FLAG_SECURE).
    virtual void setSecureScreen(bool on) { Q_UNUSED(on); }

    //An admin code push arrived or was tapped (codeId may be empty: "look at
    //the stash"). Kept until the code sheet is up (takePendingAdminCode()),
    //and adminCodeNotified() is emitted.
    void handleAdminCodeNotified(const QString &codeId);
    QString takePendingAdminCode();
    bool hasPendingAdminCode() const { return !pendingAdminCodeId.isNull(); }

    //let the HardwareUtils class know about the connection state
    void updateCalaosConnectState(bool connected) { calaosConnected = connected; }
    bool isCalaosConnected() { return calaosConnected; }

    //This is used by private classes
    void emitNetworkStatusChanged();
    void emitApplicationActiveChanged(bool active);
    void emitDialogTextValid(const QString &s);
    void emitDialogCancel();
    //Platform answers of requestNativePassword(): only the pending request's
    //first answer goes through.
    void emitNativePassword(const QString &purpose, const QString &user, const QString &password);
    void emitNativePasswordCanceled(const QString &purpose);
    void setInlineEntryPurpose(const QString &purpose) { inlineEntryPurpose = purpose; }
    void emitCalaosServerDetected();
    void emitPushNotifReceived(const QString &uuid);

signals:
    void networkStatusChanged();
    void applicationWillResignActive();
    void applicationBecomeActive();

    void dialogCanceled();
    void dialogTextValid(const QString &text);

    void nativePasswordEntered(const QString &purpose, const QString &user, const QString &password);
    void nativePasswordCanceled(const QString &purpose);

    //emited when a calaos_server has been detected on the LAN
    void calaosServerDetected();

    //emited when in foreground and a push notif has been received
    void pushNotifReceived(const QString &uuid);

    //emited when the push token changed (see setDeviceToken())
    void deviceTokenChanged();

    //emited when effectiveDeviceName() changed (see setDeviceNameOverride())
    void deviceNameChanged();

    //emited when pushKey() changed (see setPushKey())
    void pushKeyChanged();

    //see handleAdminCodeNotified()
    void adminCodeNotified(const QString &codeId);
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

//How a push that arrives while the app is in the foreground is shown by the
//system. Header-only so that tests/tst_pushnotif can check it.
struct ForegroundPushPresentation
{
    bool banner = false;
    bool list = false;
    bool sound = false;
};

//The app only shows an event push itself (event viewer) when logged in; an
//encrypted admin code opens the code sheet whatever the connection (the
//phone never needs the box to read it). Anything else, like the legacy
//admin code text, would be invisible without a banner.
//The badge is never set here: the user is looking at the app.
inline ForegroundPushPresentation foregroundPushPresentation(bool hasEventUuid, bool connected,
                                                             bool hasAdminCode = false)
{
    ForegroundPushPresentation p;
    p.banner = !(hasEventUuid && connected) && !hasAdminCode;
    p.list = true;
    p.sound = true;
    return p;
}

#endif // HARDWAREUTILS_H
