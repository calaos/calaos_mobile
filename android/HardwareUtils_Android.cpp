#include "HardwareUtils_Android.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QDebug>
#include <QPointer>
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

//A push token is a per-device credential: never log it in full.
QString describeToken(const QString &token)
{
    if (token.isEmpty())
        return QStringLiteral("<empty>");
    return QStringLiteral("%1... (%2 chars)").arg(token.left(8)).arg(token.size());
}

//Called from Android threads (UI thread, FCM binder thread): only hop to
//the Qt thread from here.
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
            startedWithNotif = true;
            notifUuid = uuid;
        }
    }
}

void HardwareUtilsAndroid::setDeviceToken(QString t)
{
    if (t.isEmpty() || t == deviceToken)
        return;

    qDebug() << "Android: FCM device token" << describeToken(t);
    deviceToken = t;
}

void HardwareUtilsAndroid::handleNotificationOpened(const QString &uuid)
{
    if (uuid.isEmpty())
        return;

    qDebug() << "Android: notification opened, event" << uuid
             << (isCalaosConnected() ? "(connected)" : "(deferred until home is loaded)");

    if (isCalaosConnected())
        emitPushNotifReceived(uuid);
    else
    {
        startedWithNotif = true;
        notifUuid = uuid;
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

int HardwareUtilsAndroid::getNetworkStatus()
{
    jint status = QJniObject::callStaticMethod<jint>(kHardwareUtilsClass,
                                                     "getNetworkStatus",
                                                     "()I");
    if (clearJavaException("HardwareUtils.getNetworkStatus"))
        status = ConnectedWWAN; //We don't know. Say it's connected.

    qDebug() << "Android: HardwareUtilsAndroid::getNetworkStatus(): " << status;

    return status;
}

void HardwareUtilsAndroid::inputTextDialog(const QString &title, const QString &message)
{
    QJniObject jTitle = QJniObject::fromString(title);
    QJniObject jMessage = QJniObject::fromString(message);

    const jboolean shown = QJniObject::callStaticMethod<jboolean>(kHardwareUtilsClass,
                                                                  "inputtextDialog",
                                                                  "(Ljava/lang/String;Ljava/lang/String;)Z",
                                                                  jTitle.object<jstring>(),
                                                                  jMessage.object<jstring>());
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
