#include "notificationservice.h"
#include <QtAndroidExtras/QAndroidJniObject>
#include <QtAndroidExtras/QAndroidJniEnvironment>
#include <jni.h>
#include <QSettings>
#include <QDebug>
#include <QtAndroid>
#include <firebase/messaging.h>
#include <QCoreApplication>
#include "HardwareUtils_Android.h"
#include "../src/Application.h"

NotificationService *NotificationService::Instance()
{
    static NotificationService instance;
    return &instance;
}

NotificationService::NotificationService(QObject *parent)
    : QObject(parent)
{
    connect(this, SIGNAL(notificationChanged()), this, SLOT(updateAndroidNotification()));
}

void NotificationService::setNotification(const QString &title, const QString &body, const QString &uuid)
{
    notifTitle = title;
    notifBody = body;
    notifEventUUID = uuid;

    emit notificationChanged();
}

void NotificationService::handleMessage(const firebase::messaging::Message &message)
{
    QString eventUUID;
    if (message.data.find("event_uuid") != message.data.end()) {
        eventUUID = QString::fromStdString(message.data.at("event_uuid"));
    }
    QString title = QString::fromStdString(message.data.at("title").c_str());
    QString body = QString::fromStdString(message.data.at("body").c_str());

    this->setNotification(title, body, eventUUID);
}

void NotificationService::updateAndroidNotification()
{
    QAndroidJniObject javaTitle = QAndroidJniObject::fromString(notifTitle);
    QAndroidJniObject javaBody= QAndroidJniObject::fromString(notifBody);
    QAndroidJniObject javaUUID = QAndroidJniObject::fromString(notifEventUUID);

    QAndroidJniObject::callStaticMethod<void>(
        "fr/calaos/calaoshome/NotificationService",
        "notify",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        QtAndroid::androidContext().object(),
        javaTitle.object<jstring>(),
        javaBody.object<jstring>(),
        javaUUID.object<jstring>()
    );
}
