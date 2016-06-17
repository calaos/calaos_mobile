#include "ScreenManager.h"
#include "XUtils.h"
#include "HardwareUtils.h"

ScreenManager::ScreenManager(QObject *parent) : QObject(parent)
{
    //disable the DPMS
    XUtils::UpdateDPMS(false, 0);

    set_dpmsEnabled(HardwareUtils::Instance()->getConfigOption("dpms_enable") == "true");
    connect(this, &ScreenManager::dpmsEnabledChanged, [](bool v)
    {
        HardwareUtils::Instance()->setConfigOption("dpms_enable", v?"true":"false");
    });

    QString time = HardwareUtils::Instance()->getConfigOption("dpms_standby");
    set_dpmsTime(time.toInt() * 60 * 1000);
    connect(this, &ScreenManager::dpmsTimeChanged, [](int v)
    {
        HardwareUtils::Instance()->setConfigOption("dpms_standby", QString::number(v / 1000.0 / 60.0));
    });
}

void ScreenManager::wakeupScreen()
{
#ifdef Q_OS_LINUX
    //Sometimes the screen wakes up but it remain black
    //doing ON->OFF->ON prevents this
    XUtils::WakeUpScreen(true);
    XUtils::WakeUpScreen(false);
#endif
    XUtils::WakeUpScreen(true);

    XUtils::UpdateDPMS(false, 0);
}

void ScreenManager::suspendScreen()
{
    XUtils::UpdateDPMS(true, 0);
    XUtils::WakeUpScreen(false);
}
