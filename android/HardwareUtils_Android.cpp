
#include "HardwareUtils_Android.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>

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
