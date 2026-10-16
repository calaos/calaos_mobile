#include "HardwareUtils_Android.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QDebug>
#include <QCoreApplication>
#include <QPointer>
#include <QNetworkInformation>
#include <jni.h>

namespace
{

constexpr char kHardwareUtilsClass[] = "fr/calaos/calaoshome/HardwareUtils";
constexpr char kNativesClass[] = "fr/calaos/calaoshome/HardwareUtilsNatives";

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

    //QJniEnvironment resolves the class through Qt's class loader, which
    //a plain FindClass from a native thread does not.
    QJniEnvironment env;
    const bool ok = env.registerNativeMethods(kNativesClass, {
        { "emitDialogTextValid", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitDialogTextValid) },
        { "emitDialogCancel", "()V", reinterpret_cast<void *>(jniEmitDialogCancel) },
        { "emitDeviceToken", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitDeviceToken) },
        { "emitNotificationOpened", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniEmitNotificationOpened) },
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
