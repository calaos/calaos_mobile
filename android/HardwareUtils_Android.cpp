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

//Called from the Android UI thread: only hop to the Qt thread from here.
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
    });
    clearJavaException("registerNativeMethods");
    if (!ok)
        qWarning() << "Android: failed to register native methods on" << kNativesClass;
}

void HardwareUtilsAndroid::setDeviceToken(QString t)
{
    qDebug() << "Received device token: " << t;
    deviceToken = t;
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
