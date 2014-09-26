
#include "../src/HardwareUtils.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>

HardwareUtils::HardwareUtils(QObject *parent):
    QObject(parent)
{
}

HardwareUtils::~HardwareUtils()
{
}

void HardwareUtils::showAlertMessage(QString title, QString message, QString buttontext)
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

void HardwareUtils::emitNetworkStatusChanged()
{
    emit networkStatusChanged();
}

int HardwareUtils::getNetworkStatus()
{
    jint status = QAndroidJniObject::callStaticMethod<jint>("fr/calaos/calaosmobile/HardwareUtils",
                                                            "getNetworkStatus");

    qDebug() << "Android: HardwareUtils::getNetworkStatus(): " << status;
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

void HardwareUtils::showNetworkActivity(bool en)
{
    Q_UNUSED(en)
}

void HardwareUtils::emitApplicationWillResignActive()
{
    emit applicationWillResignActive();
}
