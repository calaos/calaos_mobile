#include "HardwareUtils.h"
#include <QSettings>
#include <QInputDialog>

#ifdef Q_OS_IOS
#include "../ios/HardwareUtils_iOS.h"
#elif defined(Q_OS_ANDROID)
#include "../android/HardwareUtils_Android.h"
#endif

HardwareUtils *HardwareUtils::Instance(QObject *parent)
{
    static HardwareUtils *hu = NULL;
    if (hu) return hu;
#ifdef Q_OS_IOS
    hu = new HardwareUtils_iOS(parent);
#elif defined(Q_OS_ANDROID)
    hu = new HardwareUtils_Android(parent);
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

void HardwareUtils::showNetworkActivity(bool en)
{
    Q_UNUSED(en)
}

void HardwareUtils::emitApplicationActiveChanged(bool active)
{
    if (active)
        emit applicationBecomeActive();
    else
        emit applicationWillResignActive();
}

void HardwareUtils::emitDialogTextValid(const QString &s)
{
    emit dialogTextValid(s);
}

void HardwareUtils::emitDialogCancel()
{
    emit dialogCanceled();
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

void HardwareUtils::inputTextDialog(const QString &title, const QString &message)
{
    QInputDialog *d = new QInputDialog();
    d->setWindowTitle(title);
    d->setInputMode(QInputDialog::TextInput);
    d->setLabelText(message);

    connect(d, &QInputDialog::rejected, [=]()
    {
        d->deleteLater();
        emit dialogCanceled();
    });
    connect(d, &QInputDialog::accepted, [=]()
    {
        d->deleteLater();
        emit dialogTextValid(d->textValue());
    });
}
