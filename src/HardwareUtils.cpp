#include "HardwareUtils.h"

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
