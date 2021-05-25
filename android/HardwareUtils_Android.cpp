
#include "HardwareUtils_Android.h"
#include "notificationservice.h"
#include "../src/Common.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <jni.h>
#include <QtAndroid>

#include <firebase/messaging.h>
#include <firebase/app.h>
#include <firebase/util.h>

static ::firebase::InitResult firebaseInitializeMessaging(::firebase::App *app, void *);

class FbListener: public ::firebase::messaging::Listener
{
public:
    FbListener()
    {
        QAndroidJniObject jniObject = QtAndroid::androidActivity();
        ::firebase::App *instance = ::firebase::App::GetInstance();
        if (instance)
        {
            fbApp = instance;
        }
        else
        {
            fbApp = ::firebase::App::Create(QAndroidJniEnvironment(), jniObject.object<jobject>());
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
        qDebug() << "Received FCM message";
        NotificationService *ns = new NotificationService();
        ns->handleMessage(message);
    }

private:
    QAndroidJniEnvironment jniEnv;
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

HardwareUtilsAndroid *HardwareUtilsAndroid::Instance(QObject *parent)
{
    return static_cast<HardwareUtilsAndroid*>(HardwareUtils::Instance(parent));
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
    QAndroidJniObject jTitle = QAndroidJniObject::fromString(title);
    QAndroidJniObject jMessage = QAndroidJniObject::fromString(message);
    QAndroidJniObject jButtontext = QAndroidJniObject::fromString(buttontext);
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils",
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
    jint status = QAndroidJniObject::callStaticMethod<jint>("fr/calaos/calaoshome/HardwareUtils",
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

    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils",
                                       "inputtextDialog",
                                       "(Ljava/lang/String;Ljava/lang/String;)V",
                                       jTitle.object<jstring>(),
                                       jMessage.object<jstring>());
}

void HardwareUtilsAndroid::loadAuthKeychain(QString &email, QString &pass)
{
    this->HardwareUtils::loadAuthKeychain(email, pass);
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils", "loadAuthKeychain");
}

void HardwareUtilsAndroid::saveAuthKeychain(const QString &email, const QString &pass)
{
    this->HardwareUtils::saveAuthKeychain(email, pass);
    QAndroidJniObject jEmail = QAndroidJniObject::fromString(email);
    QAndroidJniObject jPass = QAndroidJniObject::fromString(pass);
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils", "saveAuthKeychain", "(Ljava/lang/String;Ljava/lang/String;)V",
                                              jEmail.object<jstring>(),
                                              jPass.object<jstring>()
                                              );
}

void HardwareUtilsAndroid::setConfigOption(QString key, QString value)
{
    this->HardwareUtils::setConfigOption(key, value);
    QAndroidJniObject jKey = QAndroidJniObject::fromString(key);
    QAndroidJniObject jValue = QAndroidJniObject::fromString(value);
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils", "setConfigOption", "(Ljava/lang/String;Ljava/lang/String;)V",
                                              jKey.object<jstring>(),
                                              jValue.object<jstring>()
                                              );
}

QString HardwareUtilsAndroid::getConfigOption(QString key)
{
    return this->HardwareUtils::getConfigOption(key);
}

void HardwareUtilsAndroid::resetAuthKeychain()
{
    this->HardwareUtils::resetAuthKeychain();
    QAndroidJniObject::callStaticMethod<void>("fr/calaos/calaoshome/HardwareUtils", "resetAuthKeychain", "()V");
}




















// JNI //

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

static jstring getDemoUser(JNIEnv *env, jobject obj)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    auto stduser = Common::getDemoUser().toStdString();
    const char* ret = stduser.c_str();
    return env->NewStringUTF(ret);
}

static jstring getDemoPass(JNIEnv *env, jobject obj)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    auto stdpass = Common::getDemoPass().toStdString();
    const char* ret = stdpass.c_str();
    return env->NewStringUTF(ret);
}

static jstring getDemoHost(JNIEnv *env, jobject obj)
{
    Q_UNUSED(env);
    Q_UNUSED(obj);
    auto stdhost = Common::getDemoHost().toStdString();
    const char* ret = stdhost.c_str();
    return env->NewStringUTF(ret);
}

static JNINativeMethod jniMethods[] =
{
    { "emitDialogTextValid", "(Ljava/lang/String;)V", reinterpret_cast<void *>(emitDialogTextValid) },
    { "emitDialogCancel", "()V", reinterpret_cast<void *>(emitDialogCancel) },
    { "getDemoUser", "()Ljava/lang/String;", reinterpret_cast<void *>(getDemoUser) },
    { "getDemoPass", "()Ljava/lang/String;", reinterpret_cast<void *>(getDemoPass) },
    { "getDemoHost", "()Ljava/lang/String;", reinterpret_cast<void *>(getDemoHost) },
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
