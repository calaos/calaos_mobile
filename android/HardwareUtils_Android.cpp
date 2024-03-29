
#include "HardwareUtils_Android.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <jni.h>
#include <QCoreApplication>

#include <firebase/messaging.h>
#include <firebase/app.h>
#include <firebase/util.h>

static ::firebase::InitResult firebaseInitializeMessaging(::firebase::App *app, void *);

class FbListener: public ::firebase::messaging::Listener
{
public:
    FbListener()
    {
        QJniObject jniObject = QNativeInterface::QAndroidApplication::context();
        ::firebase::App *instance = ::firebase::App::GetInstance();
        if (instance)
        {
            fbApp = instance;
        }
        else
        {
            fbApp = ::firebase::App::Create(QJniEnvironment().jniEnv(), jniObject.object<jobject>());
        }
    }

    void initMessaging()
    {
        qDebug() << "Initializing Firebase module";
        fbInitializer.Initialize(fbApp, nullptr, firebaseInitializeMessaging);
        qDebug() << "Module initialized. Waiting on messaging initialization";

        firebase::messaging::GetToken().OnCompletion([](const firebase::Future<std::string> &completed_future)
        {
            if (completed_future.status() != firebase::kFutureStatusComplete)
            {
                qDebug() << "FB: GetToken future status not completed";
                return;
            }

            std::string token = *completed_future.result();
            HardwareUtilsAndroid *o = reinterpret_cast<HardwareUtilsAndroid *>(HardwareUtils::Instance());
            o->setDeviceToken(QString::fromStdString(token));
        });
    }

    virtual void OnTokenReceived(const char *token)
    {
        HardwareUtilsAndroid *o = reinterpret_cast<HardwareUtilsAndroid *>(HardwareUtils::Instance());
        o->setDeviceToken(token);
    }

    virtual void OnMessage(const ::firebase::messaging::Message &message)
    {
        Q_UNUSED(message)
        qDebug() << "Received FCM message";
    }

private:
    QJniEnvironment jniEnv;
    ::firebase::App* fbApp;
    ::firebase::ModuleInitializer fbInitializer;
};

static ::firebase::InitResult firebaseInitializeMessaging(::firebase::App *app, void *)
{
    qDebug() << "Try to initialize Firebase Messaging";
    HardwareUtilsAndroid *o = reinterpret_cast<HardwareUtilsAndroid *>(HardwareUtils::Instance());
    auto res = ::firebase::messaging::Initialize(*app, o->getFbListener());
    if (res == firebase::kInitResultSuccess)
        qDebug() << "FB Messaging Init success";
    if (res == firebase::kInitResultFailedMissingDependency)
        qWarning() << "FB Messaging init failed: missing dependency (google play services)";
    return res;
}

HardwareUtilsAndroid::HardwareUtilsAndroid(QObject *parent):
    HardwareUtils(parent),
    fcmListener(new FbListener())
{
}

HardwareUtilsAndroid::~HardwareUtilsAndroid()
{
}

void HardwareUtilsAndroid::platformInit(QQmlApplicationEngine *e)
{
    HardwareUtils::platformInit(e);

    //init FCM
    fcmListener->initMessaging();
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
    QJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils",
                                       "showAlertMessage",
                                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>(),
                                       jButtontext.object<jstring>());

    //Clear exception if any
    QJniEnvironment env;
    if (env->ExceptionCheck())
    {
        qDebug() << "JNI call failed";
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

int HardwareUtilsAndroid::getNetworkStatus()
{
    jint status = QJniObject::callStaticMethod<jint>("fr/calaos/calaoshome/HardwareUtils",
                                                     "getNetworkStatus");

    qDebug() << "Android: HardwareUtilsAndroid::getNetworkStatus(): " << status;
    //Clear exception if any
    QJniEnvironment env;
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
    QJniObject jTitle = QJniObject::fromString(title);
    QJniObject jMessage = QJniObject::fromString(message);

    QJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils",
                                       "inputtextDialog",
                                       "(Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>());
}

static void emitDialogTextValid(JNIEnv *env, jobject obj, jstring text)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    QJniObject jnitext(text);
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
    jclass javaClass = env->FindClass("fr/calaos/calaoshome/HardwareUtilsNatives");
    if (!javaClass)
        return JNI_ERR;

    // register our native methods
    jint res;
    res = env->RegisterNatives(javaClass,
                             jniMethods,
                             sizeof(jniMethods) / sizeof(jniMethods[0]));
    if (res < 0)
    {
        qDebug() << "RegisterNatives failed with code: " << res;
        env->DeleteLocalRef(javaClass);
        return JNI_ERR;
    }

    env->DeleteLocalRef(javaClass);
    return JNI_VERSION_1_6;
}
