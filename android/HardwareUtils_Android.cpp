#include "HardwareUtils_Android.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QNetworkInformation>
#include <QSettings>
#include <jni.h>

namespace
{

constexpr char kHardwareUtilsClass[] = "fr/calaos/calaoshome/HardwareUtils";
constexpr char kNativesClass[] = "fr/calaos/calaoshome/HardwareUtilsNatives";
constexpr char kCredentialStoreClass[] = "fr/calaos/calaoshome/CredentialStore";
constexpr char kMessagingClass[] = "fr/calaos/calaoshome/CalaosMessagingService";

//QSettings keys of the clear-text credentials written by the base class
//(and by this app before AND-7): read once for the migration, then removed.
const QLatin1StringView kSettingsUser("calaos/cn_user");
const QLatin1StringView kSettingsPass("calaos/cn_pass");

QJniObject androidContext()
{
    return QNativeInterface::QAndroidApplication::context();
}

QString credentialStoreString(const char *method)
{
    QJniObject ctx = androidContext();
    QJniObject s = QJniObject::callStaticObjectMethod(kCredentialStoreClass, method,
                                                      "(Landroid/content/Context;)Ljava/lang/String;",
                                                      ctx.object());
    QJniEnvironment env;
    if (env.checkAndClearExceptions(QJniEnvironment::OutputMode::Verbose) || !s.isValid())
    {
        qWarning() << "Android: CredentialStore call failed:" << method;
        return QString();
    }
    return s.toString();
}

//Removes the clear-text pair from QSettings, if any is left.
void forgetSettingsCredentials()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (!settings.contains(kSettingsUser) && !settings.contains(kSettingsPass))
        return;
    settings.remove(kSettingsUser);
    settings.remove(kSettingsPass);
    settings.sync();
}

//The live instance, read by the native callbacks. Becomes null once the
//object is destroyed (Application teardown), so a late callback is dropped
//instead of touching a dangling pointer.
QPointer<HardwareUtilsAndroid> s_instance;

//Describe (in logcat) and clear any pending Java exception left by the last
//JNI call. Returns true if there was one.
bool clearJavaException(const char *call)
{
    QJniEnvironment env;
    if (env.checkAndClearExceptions(QJniEnvironment::OutputMode::Verbose))
    {
        qWarning() << "Android: JNI call failed:" << call;
        return true;
    }
    return false;
}

//Called from Android threads (UI thread, the FCM SDK's intent-handler
//executor thread): only hop to the Qt thread from here.
void jniEmitDialogTextValid(JNIEnv *, jclass, jstring text)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString s = QJniObject(text).toString();
    QMetaObject::invokeMethod(hw, [hw, s]()
    {
        hw->emitDialogTextValid(s);
    }, Qt::QueuedConnection);
}

void jniEmitDialogCancel(JNIEnv *, jclass)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    QMetaObject::invokeMethod(hw, [hw]()
    {
        hw->emitDialogCancel();
    }, Qt::QueuedConnection);
}

void jniEmitBiometricStoreDone(JNIEnv *, jclass, jstring key, jint result)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString k = QJniObject(key).toString();
    QMetaObject::invokeMethod(hw, [hw, k, result]()
    {
        hw->emitBiometricStoreDone(k, result);
    }, Qt::QueuedConnection);
}

void jniEmitBiometricReadDone(JNIEnv *, jclass, jstring key, jint result, jstring secret)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString k = QJniObject(key).toString();
    const QString s = QJniObject(secret).toString();
    QMetaObject::invokeMethod(hw, [hw, k, result, s]()
    {
        hw->emitBiometricReadDone(k, result, s);
    }, Qt::QueuedConnection);
}

void jniEmitDeviceToken(JNIEnv *, jclass, jstring token)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString s = QJniObject(token).toString();
    QMetaObject::invokeMethod(hw, [hw, s]()
    {
        hw->setDeviceToken(s);
    }, Qt::QueuedConnection);
}

void jniEmitNotificationOpened(JNIEnv *, jclass, jstring uuid)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString s = QJniObject(uuid).toString();
    QMetaObject::invokeMethod(hw, [hw, s]()
    {
        hw->handleNotificationOpened(s);
    }, Qt::QueuedConnection);
}

void jniEmitAdminCodeNotified(JNIEnv *, jclass, jstring codeId)
{
    HardwareUtilsAndroid *hw = s_instance.data();
    if (!hw)
        return;

    const QString s = QJniObject(codeId).toString();
    QMetaObject::invokeMethod(hw, [hw, s]()
    {
        hw->handleAdminCodeNotified(s);
    }, Qt::QueuedConnection);
}

} // namespace

HardwareUtilsAndroid::HardwareUtilsAndroid(QObject *parent):
    HardwareUtils(parent)
{
    s_instance = this;
}

HardwareUtilsAndroid::~HardwareUtilsAndroid()
{
}

void HardwareUtilsAndroid::platformInit(QQmlApplicationEngine *e)
{
    HardwareUtils::platformInit(e);

    //Independent of the JNI registration below: a failure there must not also
    //disable network monitoring.
    setupNetworkMonitoring();

    //Created on the first run; read before the first login sends it.
    loadPushKey();

    //QJniEnvironment resolves the class through Qt's class loader, which
    //a plain FindClass from a native thread does not.
    QJniEnvironment env;
    const bool ok = env.registerNativeMethods(kNativesClass, {
        { "emitDialogTextValid", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitDialogTextValid) },
        { "emitDialogCancel", "()V", reinterpret_cast<void *>(jniEmitDialogCancel) },
        { "emitBiometricStoreDone", "(Ljava/lang/String;I)V", reinterpret_cast<void *>(jniEmitBiometricStoreDone) },
        { "emitBiometricReadDone", "(Ljava/lang/String;ILjava/lang/String;)V", reinterpret_cast<void *>(jniEmitBiometricReadDone) },
        { "emitDeviceToken", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitDeviceToken) },
        { "emitNotificationOpened", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitNotificationOpened) },
        { "emitAdminCodeNotified", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitAdminCodeNotified) },
    });
    clearJavaException("registerNativeMethods");
    if (!ok)
    {
        qWarning() << "Android: failed to register native methods on" << kNativesClass;
        return;
    }

    //From now on Java forwards token refreshes and notification taps.
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "setNativesReady", "()V");
    clearJavaException("HardwareUtils.setNativesReady");

    //Token saved by a previous run (or by the messaging service while Qt was
    //not loaded): available right away, so the first login can register it.
    QJniObject jtoken = QJniObject::callStaticObjectMethod(kHardwareUtilsClass,
                                                           "getStoredPushToken",
                                                           "()Ljava/lang/String;");
    if (!clearJavaException("HardwareUtils.getStoredPushToken") && jtoken.isValid())
        setDeviceToken(jtoken.toString());

    //Then ask Firebase for the current one; it comes back through
    //emitDeviceToken() (first run, or rotated token).
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "requestPushToken", "()V");
    clearJavaException("HardwareUtils.requestPushToken");

    //Cold start from an admin code notification: read once the UI is up.
    QJniObject jcode = QJniObject::callStaticObjectMethod(kHardwareUtilsClass,
                                                          "takePendingAdminCodeId",
                                                          "()Ljava/lang/String;");
    if (!clearJavaException("HardwareUtils.takePendingAdminCodeId") && jcode.isValid() &&
        !jcode.toString().isEmpty())
        handleAdminCodeNotified(jcode.toString());

    //Cold start from a notification tap: the activity parked the uuid.
    QJniObject juuid = QJniObject::callStaticObjectMethod(kHardwareUtilsClass,
                                                          "takePendingEventUuid",
                                                          "()Ljava/lang/String;");
    if (!clearJavaException("HardwareUtils.takePendingEventUuid") && juuid.isValid())
    {
        const QString uuid = juuid.toString();
        if (!uuid.isEmpty())
        {
            qDebug() << "Android: started from notification, event" << uuid;
            //Not logged in yet: kept until home is loaded.
            handleNotificationOpened(uuid);
        }
    }
}

void HardwareUtilsAndroid::showAlertMessage(QString title, QString message, QString buttontext)
{
    QJniObject jTitle = QJniObject::fromString(title);
    QJniObject jMessage = QJniObject::fromString(message);
    QJniObject jButtontext = QJniObject::fromString(buttontext);
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass,
                                       "showAlertMessage",
                                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>(),
                                       jButtontext.object<jstring>());
    clearJavaException("HardwareUtils.showAlertMessage");
}

void HardwareUtilsAndroid::setupNetworkMonitoring()
{
    //The backend plugin (libplugins_networkinformation_qandroidnetworkinformation)
    //is deployed by androiddeployqt along with Qt6::Network.
    if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability |
                                                    QNetworkInformation::Feature::TransportMedium) &&
        !QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability))
    {
        qWarning() << "Android: no QNetworkInformation backend, network changes will not be reported";
        return;
    }

    QNetworkInformation *ni = QNetworkInformation::instance();
    m_lastNetworkStatus = getNetworkStatus();
    qInfo() << "Android: network monitoring backend" << ni->backendName()
            << "reachability" << ni->reachability()
            << "medium" << ni->transportMedium();

    //Reachability covers airplane mode / link lost / link back; a medium
    //change covers Wi-Fi <-> cellular handovers that keep us "online".
    connect(ni, &QNetworkInformation::reachabilityChanged, this,
            [this](QNetworkInformation::Reachability r)
    {
        qInfo() << "Android: reachability changed" << r;
        emitNetworkStatusIfChanged();
    });
    connect(ni, &QNetworkInformation::transportMediumChanged, this,
            [this](QNetworkInformation::TransportMedium m)
    {
        qInfo() << "Android: transport medium changed" << m;
        emitNetworkStatusIfChanged();
    });
}

//Status as seen by QNetworkInformation, the source of the emitted changes, so
//getNetworkStatus() never contradicts the last networkStatusChanged().
int HardwareUtilsAndroid::networkStatusFromQt(bool *known) const
{
    *known = false;
    QNetworkInformation *ni = QNetworkInformation::instance();
    if (!ni)
        return NotConnected;

    switch (ni->reachability())
    {
    case QNetworkInformation::Reachability::Disconnected:
        *known = true;
        return NotConnected;
    case QNetworkInformation::Reachability::Unknown:
        return NotConnected; //not known yet, caller falls back on Java
    default:
        break;
    }

    *known = true;
    switch (ni->transportMedium())
    {
    case QNetworkInformation::TransportMedium::WiFi:
    case QNetworkInformation::TransportMedium::Ethernet:
        return ConnectedWiFi;
    default:
        return ConnectedWWAN;
    }
}

//Reachability and transport medium often both change for a single network
//event; the status is what the app acts on, so emit once per actual change.
void HardwareUtilsAndroid::emitNetworkStatusIfChanged()
{
    const int status = getNetworkStatus();
    if (status == m_lastNetworkStatus)
        return;

    m_lastNetworkStatus = status;
    emitNetworkStatusChanged();
}

QString HardwareUtilsAndroid::getDeviceName()
{
    QJniObject jname = QJniObject::callStaticObjectMethod(kHardwareUtilsClass,
                                                          "getDeviceName",
                                                          "()Ljava/lang/String;");
    if (clearJavaException("HardwareUtils.getDeviceName") || !jname.isValid())
        return QString();
    return jname.toString();
}

int HardwareUtilsAndroid::getNetworkStatus()
{
    bool known = false;
    int status = networkStatusFromQt(&known);

    if (!known)
    {
        status = QJniObject::callStaticMethod<jint>(kHardwareUtilsClass,
                                                    "getNetworkStatus",
                                                    "()I");
        if (clearJavaException("HardwareUtils.getNetworkStatus"))
            status = ConnectedWWAN; //We don't know. Say it's connected.
    }

    qDebug() << "Android: HardwareUtilsAndroid::getNetworkStatus(): " << status
             << (known ? "(qt)" : "(java)");

    return status;
}

void HardwareUtilsAndroid::inputTextDialog(const QString &title, const QString &message)
{
    QJniObject jTitle = QJniObject::fromString(title);
    QJniObject jMessage = QJniObject::fromString(message);

    //Translated here, with the language chosen in the app (Application::setLanguage),
    //not by Android resources: the app language is independent of the device locale.
    QJniObject jConfirm = QJniObject::fromString(
        QCoreApplication::translate("HardwareUtilsAndroid", "Confirm"));
    QJniObject jCancel = QJniObject::fromString(
        QCoreApplication::translate("HardwareUtilsAndroid", "Cancel"));

    const jboolean shown = QJniObject::callStaticMethod<jboolean>(kHardwareUtilsClass,
                                                                  "inputtextDialog",
                                                                  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
                                                                  jTitle.object<jstring>(),
                                                                  jMessage.object<jstring>(),
                                                                  jConfirm.object<jstring>(),
                                                                  jCancel.object<jstring>());
    const bool failed = clearJavaException("HardwareUtils.inputtextDialog");

    //No dialog means no answer will ever come back: report a cancel so the
    //caller does not keep waiting for one. Queued like every other dialog
    //answer, so it never arrives inside the caller's own call.
    if (failed || !shown)
    {
        QMetaObject::invokeMethod(this, [this]()
        {
            emitDialogCancel();
        }, Qt::QueuedConnection);
    }
}

bool HardwareUtilsAndroid::hasBiometrics() const
{
    const jboolean available = QJniObject::callStaticMethod<jboolean>(kHardwareUtilsClass, "biometricsAvailable", "()Z");
    return !clearJavaException("HardwareUtils.biometricsAvailable") && available;
}

void HardwareUtilsAndroid::biometricStore(const QString &key, const QString &secret, const QString &reason)
{
    QJniObject jKey = QJniObject::fromString(key), jSecret = QJniObject::fromString(secret),
               jTitle = QJniObject::fromString(reason), jCancel = QJniObject::fromString(tr("Cancel"));
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "biometricStore",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        jKey.object<jstring>(), jSecret.object<jstring>(), jTitle.object<jstring>(), jCancel.object<jstring>());
    clearJavaException("HardwareUtils.biometricStore");
}

void HardwareUtilsAndroid::biometricRead(const QString &key, const QString &reason)
{
    QJniObject jKey = QJniObject::fromString(key), jTitle = QJniObject::fromString(reason),
               jCancel = QJniObject::fromString(tr("Cancel"));
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "biometricRead",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        jKey.object<jstring>(), jTitle.object<jstring>(), jCancel.object<jstring>());
    clearJavaException("HardwareUtils.biometricRead");
}

void HardwareUtilsAndroid::biometricCancelStore()
{
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "biometricCancelStore", "()V");
    clearJavaException("HardwareUtils.biometricCancelStore");
}

//Through the application context: nothing may depend on a live activity.
void HardwareUtilsAndroid::biometricRemove(const QString &key)
{
    QJniObject ctx = androidContext();
    QJniObject jKey = QJniObject::fromString(key);
    QJniObject::callStaticMethod<void>(kCredentialStoreClass, "biometricRemove",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       ctx.object<jobject>(), jKey.object<jstring>());
    clearJavaException("CredentialStore.biometricRemove");
}

bool HardwareUtilsAndroid::biometricRemoveAll()
{
    QJniObject ctx = androidContext();
    const jboolean ok = QJniObject::callStaticMethod<jboolean>(kCredentialStoreClass, "biometricRemoveAll",
                                                               "(Landroid/content/Context;)Z", ctx.object<jobject>());
    return !clearJavaException("CredentialStore.biometricRemoveAll") && ok;
}

//Returns false when Java could not store the pair (Keystore failure).
static bool storeCredentials(const QString &user, const QString &pass)
{
    QJniObject ctx = androidContext();
    QJniObject juser = QJniObject::fromString(user);
    QJniObject jpass = QJniObject::fromString(pass);
    const jboolean ok = QJniObject::callStaticMethod<jboolean>(kCredentialStoreClass, "save",
                                                               "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Z",
                                                               ctx.object(),
                                                               juser.object<jstring>(),
                                                               jpass.object<jstring>());
    if (clearJavaException("CredentialStore.save"))
        return false;
    return ok;
}

void HardwareUtilsAndroid::saveAuthKeychain(const QString &email, const QString &pass)
{
    if (!storeCredentials(email, pass))
    {
        //Keep the user logged in at the next start rather than losing the
        //pair: the base class copy (QSettings) is migrated again later. The
        //store must not keep a previous pair meanwhile, or loadAuthKeychain()
        //would hand that stale one out and drop the new one from QSettings.
        qWarning() << "Android: credentials kept in settings, secure store unavailable";
        QJniObject ctx = androidContext();
        QJniObject::callStaticMethod<void>(kCredentialStoreClass, "clearCredentials",
                                           "(Landroid/content/Context;)V", ctx.object());
        clearJavaException("CredentialStore.clearCredentials");
        HardwareUtils::saveAuthKeychain(email, pass);
        return;
    }
    forgetSettingsCredentials();
}

void HardwareUtilsAndroid::setSessionActive(bool active)
{
    //Read by PushPictureWorker: after an explicit logout the stored pair
    //stays for the login screen but is not used for pictures any more.
    QJniObject ctx = androidContext();
    QJniObject::callStaticMethod<void>(kCredentialStoreClass, "setSessionActive",
                                       "(Landroid/content/Context;Z)V", ctx.object(),
                                       static_cast<jboolean>(active));
    clearJavaException("CredentialStore.setSessionActive");
}

void HardwareUtilsAndroid::setTlsPins(const QVariantMap &pins)
{
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(pins)).toJson(QJsonDocument::Compact);
    QJniObject ctx = androidContext();
    QJniObject jpins = QJniObject::fromString(QString::fromUtf8(json));
    QJniObject::callStaticMethod<void>(kCredentialStoreClass, "setTlsPins",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       ctx.object(), jpins.object<jstring>());
    clearJavaException("CredentialStore.setTlsPins");
}

bool HardwareUtilsAndroid::isSessionActive()
{
    //Any failure (class or method not resolved, Java exception, no context)
    //answers true, the pre-FIX-11 behaviour (automatic login allowed), rather
    //than locking the user out of the automatic login for good. Raw JNIEnv on
    //purpose: QJniObject::callStaticMethod<jboolean> clears a pending
    //exception itself and returns false, which would read as "logged out".
    QJniObject ctx = androidContext();
    if (!ctx.isValid())
        return true;

    QJniEnvironment env;
    jclass cls = env.findClass(kCredentialStoreClass);
    if (!cls)
    {
        env.checkAndClearExceptions(QJniEnvironment::OutputMode::Verbose);
        qWarning() << "Android: CredentialStore not found, session considered active";
        return true;
    }
    jmethodID mid = env.findStaticMethod(cls, "isSessionActive", "(Landroid/content/Context;)Z");
    if (!mid)
    {
        env.checkAndClearExceptions(QJniEnvironment::OutputMode::Verbose);
        qWarning() << "Android: CredentialStore.isSessionActive not found, session considered active";
        return true;
    }

    const jboolean active = env->CallStaticBooleanMethod(cls, mid, ctx.object());
    if (env.checkAndClearExceptions(QJniEnvironment::OutputMode::Verbose))
    {
        qWarning() << "Android: JNI call failed: CredentialStore.isSessionActive, session considered active";
        return true;
    }
    return active == JNI_TRUE;
}

void HardwareUtilsAndroid::loadAuthKeychain(QString &email, QString &pass)
{
    email = credentialStoreString("user");
    pass = credentialStoreString("password");

    if (email.isEmpty() && pass.isEmpty())
    {
        //First start after the update (or a failed save, see above): the pair
        //is still in QSettings. Move it, and only then drop the clear copy.
        QString u, p;
        HardwareUtils::loadAuthKeychain(u, p);
        if (!u.isEmpty() || !p.isEmpty())
        {
            if (storeCredentials(u, p))
            {
                qDebug() << "Android: credentials migrated to the secure store";
                forgetSettingsCredentials();
            }
            email = u;
            pass = p;
        }
    }
    else
    {
        forgetSettingsCredentials();
    }

    //The worker also needs the server address; setConfigOption() keeps it
    //up to date from now on, this covers the install being updated.
    if (credentialStoreString("host").isEmpty())
        setConfigOption(QStringLiteral("calaos/host"), getServerHost());
}

void HardwareUtilsAndroid::resetAuthKeychain()
{
    //"Reset all data": nothing stays behind, in Java or in QSettings (the
    //base class would write the demo pair there; loadSettings() falls back
    //to it when the store is empty).
    QJniObject ctx = androidContext();
    QJniObject::callStaticMethod<void>(kCredentialStoreClass, "clear",
                                       "(Landroid/content/Context;)V", ctx.object());
    clearJavaException("CredentialStore.clear");
    forgetSettingsCredentials();
}

void HardwareUtilsAndroid::loadPushKey()
{
    const QByteArray key = QByteArray::fromHex(credentialStoreString("pushKey").toLatin1());
    if (key.size() == 32)
        setPushKey(key);
    else
        qWarning() << "Android: push key not available now, asked again later";
}

HardwareUtils::SecretCopy HardwareUtilsAndroid::copySecret(const QString &text, int seconds)
{
    Q_UNUSED(seconds) //cleared by AdminCodeModel
    QJniObject jtext = QJniObject::fromString(text);
    const jboolean ok = QJniObject::callStaticMethod<jboolean>(kHardwareUtilsClass, "copySecret",
                                                               "(Ljava/lang/String;)Z",
                                                               jtext.object<jstring>());
    if (clearJavaException("HardwareUtils.copySecret") || !ok)
        return SecretCopy::NotHandled;
    return SecretCopy::Copied;
}

void HardwareUtilsAndroid::setSecureScreen(bool on)
{
    QJniObject::callStaticMethod<void>(kHardwareUtilsClass, "setSecureScreen", "(Z)V",
                                       static_cast<jboolean>(on));
    clearJavaException("HardwareUtils.setSecureScreen");
}

QList<QJsonObject> HardwareUtilsAndroid::adminCodeStash()
{
    QList<QJsonObject> entries;
    const QJsonArray arr = QJsonDocument::fromJson(credentialStoreString("adminCodes").toUtf8()).array();
    for (const QJsonValue &v: arr)
        if (v.isObject())
            entries << v.toObject();
    return entries;
}

void HardwareUtilsAndroid::removeAdminCodes(const QStringList &codeIds)
{
    if (codeIds.isEmpty())
        return;
    const QByteArray json = QJsonDocument(QJsonArray::fromStringList(codeIds)).toJson(QJsonDocument::Compact);
    QJniObject ctx = androidContext();
    QJniObject jids = QJniObject::fromString(QString::fromUtf8(json));
    QJniObject::callStaticMethod<void>(kMessagingClass, "consumeAdminCodes",
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       ctx.object(), jids.object<jstring>());
    clearJavaException("CalaosMessagingService.consumeAdminCodes");
}

void HardwareUtilsAndroid::setConfigOption(QString key, QString value)
{
    if (key == QLatin1String("calaos/host"))
    {
        //Shared with the push picture worker, like calaos.hostname in the
        //iOS App Group.
        QJniObject ctx = androidContext();
        QJniObject jhost = QJniObject::fromString(value);
        QJniObject::callStaticMethod<void>(kCredentialStoreClass, "setHost",
                                           "(Landroid/content/Context;Ljava/lang/String;)V",
                                           ctx.object(), jhost.object<jstring>());
        clearJavaException("CredentialStore.setHost");
    }

    HardwareUtils::setConfigOption(key, value);
}
