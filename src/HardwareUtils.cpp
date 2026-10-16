#include "HardwareUtils.h"
#include <QSettings>
#include "Common.h"

#ifdef Q_OS_IOS
#include "../ios/HardwareUtils_iOS.h"
#elif defined(Q_OS_ANDROID)
#include "../android/HardwareUtils_Android.h"
#else
#include <QInputDialog>
#endif

#ifdef CALAOS_DESKTOP
#include "HardwareUtils_desktop.h"
#endif

HardwareUtils *HardwareUtils::Instance(QObject *parent)
{
    static HardwareUtils *hu = nullptr;
    if (hu) return hu;
#ifdef Q_OS_IOS
    hu = new HardwareUtils_iOS(parent);
#elif defined(Q_OS_ANDROID)
    hu = new HardwareUtilsAndroid(parent);
#elif defined(CALAOS_DESKTOP)
    hu = new HardwareUtilsDesktop(parent);
#else
    hu = new HardwareUtils(parent);
#endif
    return hu;
}

HardwareUtils::HardwareUtils(QObject *parent):
    QObject(parent)
{
}

HardwareUtils::~HardwareUtils()
{
}

void HardwareUtils::platformInit(QQmlApplicationEngine *e)
{
    qmlEngine = e;
}

void HardwareUtils::showAlertMessage(QString title, QString message, QString buttontext)
{
    Q_UNUSED(title)
    Q_UNUSED(message)
    Q_UNUSED(buttontext)
}

void HardwareUtils::emitNetworkStatusChanged()
{
    emit networkStatusChanged();
}

int HardwareUtils::getNetworkStatus()
{
    return ConnectedWWAN;
}

void HardwareUtils::emitApplicationActiveChanged(bool active)
{
    if (active)
        emit applicationBecomeActive();
    else
    {
        //reset started with options, so we do not
        //restart last option next time we "wake up" the app (not being killed)
        startedWithOpt = false;

        emit applicationWillResignActive();
    }
}

void HardwareUtils::emitDialogTextValid(const QString &s)
{
    emit dialogTextValid(s);
}

void HardwareUtils::emitDialogCancel()
{
    emit dialogCanceled();
}

void HardwareUtils::emitCalaosServerDetected()
{
    emit calaosServerDetected();
}

void HardwareUtils::emitPushNotifReceived(const QString &uuid)
{
    emit pushNotifReceived(uuid);
}

void HardwareUtils::handleNotificationOpened(const QString &uuid)
{
    if (uuid.isEmpty())
        return;

    qDebug() << "Notification opened, event" << uuid
             << (calaosConnected ? "(connected)" : "(deferred until home is loaded)");

    if (calaosConnected)
    {
        //An older deferred tap is superseded by the one being opened now.
        pendingNotifUuid.clear();
        emit pushNotifReceived(uuid);
    }
    else
    {
        pendingNotifUuid = uuid;
    }
}

QString HardwareUtils::takePendingNotif()
{
    QString uuid;
    uuid.swap(pendingNotifUuid);
    return uuid;
}

void HardwareUtils::restorePendingNotif(const QString &uuid)
{
    if (uuid.isEmpty() || !pendingNotifUuid.isEmpty())
        return;

    qDebug() << "Notification" << uuid << "not opened before the session dropped, kept for the next login";
    pendingNotifUuid = uuid;
}

void HardwareUtils::forgetPendingUserActions()
{
    if (!pendingNotifUuid.isEmpty() || startedWithOpt)
        qDebug() << "Explicit logout: pending notification tap and start option dropped";

    pendingNotifUuid.clear();
    startedWithOpt = false;
}

void HardwareUtils::setDeviceToken(const QString &token)
{
    if (token.isEmpty() || token == deviceToken)
        return;

    qDebug() << "Push device token" << describeToken(token);
    deviceToken = token;
    emit deviceTokenChanged();
}

void HardwareUtils::clearDeviceToken()
{
    if (deviceToken.isEmpty())
        return;

    qDebug() << "Push device token cleared";
    deviceToken.clear();
    //Nothing is sent for an empty token: the next one is registered as usual.
    emit deviceTokenChanged();
}

QString HardwareUtils::describeToken(const QString &token)
{
    if (token.isEmpty())
        return QStringLiteral("<empty>");
    return QStringLiteral("%1... (%2 chars)").arg(token.left(8)).arg(token.size());
}

void HardwareUtils::loadAuthKeychain(QString &email, QString &pass)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    email = settings.value("calaos/cn_user").toString();
    pass = settings.value("calaos/cn_pass").toString();
}

void HardwareUtils::saveAuthKeychain(const QString &email, const QString &pass)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    settings.setValue("calaos/cn_user", email);
    settings.setValue("calaos/cn_pass", pass);

    settings.sync();
}

QString HardwareUtils::getServerHost()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    return settings.value("calaos/host", Common::getDemoHost()).toString();
}

void HardwareUtils::resetAuthKeychain()
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    settings.setValue("calaos/cn_user", Common::getDemoUser());
    settings.setValue("calaos/cn_pass", Common::getDemoPass());

    settings.sync();
}

void HardwareUtils::setConfigOption(QString key, QString value)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    settings.setValue(QStringLiteral("config/%1").arg(key), value);
    settings.sync();
}

QString HardwareUtils::getConfigOption(QString key)
{
    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return settings.value(QStringLiteral("config/%1").arg(key)).toString();
}

void HardwareUtils::inputTextDialog(const QString &title, const QString &message)
{
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
    QInputDialog *d = new QInputDialog();
    d->setWindowTitle(title);
    d->setInputMode(QInputDialog::TextInput);
    d->setLabelText(message);

    connect(d, &QInputDialog::rejected, this, [=]()
    {
        d->deleteLater();
        emit dialogCanceled();
    });
    connect(d, &QInputDialog::accepted, this, [=]()
    {
        d->deleteLater();
        emit dialogTextValid(d->textValue());
    });

    d->setModal(true);
    d->show();
#else
    Q_UNUSED(title);
    Q_UNUSED(message);
#endif
}
