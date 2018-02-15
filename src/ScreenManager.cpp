#include "ScreenManager.h"
#include "XUtils.h"
#include "HardwareUtils.h"

ScreenManager::ScreenManager(QObject *parent) : QObject(parent)
{
    //disable the DPMS
    XUtils::UpdateDPMS(false, 0);

    set_dpmsEnabled(HardwareUtils::Instance()->getConfigOption("dpms_enable") == "true");
    QString time = HardwareUtils::Instance()->getConfigOption("dpms_standby");
    set_dpmsTime(time.toInt() * 60 * 1000);

    if (get_dpmsTime() <= 0)
        set_dpmsTime(1 * 60 * 1000);
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

void ScreenManager::updateDpmsEnabled(bool en)
{
    set_dpmsEnabled(en);
    scheduleWriteConf();
}

void ScreenManager::updateDpmsTime(int timeMin)
{
    if (timeMin <= 0)
        timeMin = 1;
    set_dpmsTime(timeMin * 60 * 1000);
    scheduleWriteConf();
}

void ScreenManager::scheduleWriteConf()
{
    delete writeConfTimer;
    writeConfTimer = new QTimer(this);

    connect(writeConfTimer, &QTimer::timeout, [=]()
    {
        HardwareUtils::Instance()->setConfigOption("dpms_enable", get_dpmsEnabled()?"true":"false");
        HardwareUtils::Instance()->setConfigOption("dpms_standby", QString::number(get_dpmsTime() / 1000.0 / 60.0));
    });

    writeConfTimer->start(5000);
}
