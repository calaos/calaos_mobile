
#include "HardwareUtils_Android.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <jni.h>

HardwareUtilsAndroid::HardwareUtilsAndroid(QObject *parent):
    HardwareUtils(parent)
{
}

HardwareUtilsAndroid::~HardwareUtilsAndroid()
{
}

void HardwareUtilsAndroid::showAlertMessage(QString title, QString message, QString buttontext)
{
    QAndroidJniObject jTitle = QAndroidJniObject::fromString(title);
    QAndroidJniObject jMessage = QAndroidJniObject::fromString(message);
    QAndroidJniObject jButtontext = QAndroidJniObject::fromString(buttontext);
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaosmobile/HardwareUtils",
                                       "showAlertMessage",
                                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>(),
                                       jButtontext.object<jstring>());

    //Clear exception if any
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck())
    {
        qDebug() << "JNI call failed";
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

int HardwareUtilsAndroid::getNetworkStatus()
{
    jint status = QAndroidJniObject::callStaticMethod<jint>("fr/calaos/calaosmobile/HardwareUtils",
                                                            "getNetworkStatus");

    qDebug() << "Android: HardwareUtilsAndroid::getNetworkStatus(): " << status;
    //Clear exception if any
    QAndroidJniEnvironment env;
    if (env->ExceptionCheck())
    {
        qDebug() << "JNI call failed";
        env->ExceptionDescribe();
        env->ExceptionClear();
    }

    return status;
}

void HardwareUtilsAndroid::inputTextDialog(const QString &title, const QString &message)
{
    QAndroidJniObject jTitle = QAndroidJniObject::fromString(title);
    QAndroidJniObject jMessage = QAndroidJniObject::fromString(message);

    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaosmobile/HardwareUtils",
                                       "inputtextDialog",
                                       "(Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>());
}

static void emitDialogTextValid(JNIEnv *env, jobject obj, jstring text)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    QAndroidJniObject jnitext(text);
    QMetaObject::invokeMethod(HardwareUtils::Instance(),
                              "emitDialogTextValid",
                              Qt::QueuedConnection,
                              Q_ARG(QString, jnitext.toString()));
}

static void emitDialogCancel(JNIEnv *env, jobject obj)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    QMetaObject::invokeMethod(HardwareUtils::Instance(),
                              "emitDialogCancel",
                              Qt::QueuedConnection);
}

static JNINativeMethod jniMethods[] =
{
    { "emitDialogTextValid", "(Ljava/lang/String;)V", reinterpret_cast<void *>(emitDialogTextValid) },
    { "emitDialogCancel", "()V", reinterpret_cast<void *>(emitDialogCancel) },
};

// this method is called automatically by Java after the .so file is loaded
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void */*reserved*/)
{
    JNIEnv *env;
    // get the JNIEnv pointer.
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
        return JNI_ERR;

    // search for Java class which declares the native methods
    jclass javaClass = env->FindClass("fr/calaos/calaosmobile/HardwareUtils");
    if (!javaClass)
        return JNI_ERR;

    // register our native methods
    if (env->RegisterNatives(javaClass,
                             jniMethods,
                             sizeof(jniMethods) / sizeof(jniMethods[0])) < 0)
    {
        env->DeleteLocalRef(javaClass);
        return JNI_ERR;
    }

    env->DeleteLocalRef(javaClass);
    return JNI_VERSION_1_6;
}
