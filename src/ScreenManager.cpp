#include "ScreenManager.h"
#include "XUtils.h"
#include "HardwareUtils.h"

ScreenManager::ScreenManager(QObject *parent) : QObject(parent)
{
    //disable the DPMS
    XUtils::UpdateDPMS(false, 0);
    startTimer();
}

ScreenManager::~ScreenManager()
{
    stopTimer();
}

int ScreenManager::getTime()
{
    QString time = HardwareUtils::Instance()->getConfigOption("dpms_standby");
    return time.toInt() * 60;
}

void ScreenManager::updateTimer()
{
    if (is_suspended)
    {
        wakeUp();
    }
    if (timer)
        timer->start(getTime() * 1000);
}

void ScreenManager::startTimer()
{
    if (timer)
        return;

    timer = new QTimer();
    connect(timer, &QTimer::timeout, [this]()
    {
        if(HardwareUtils::Instance()->getConfigOption("dpms_enable") == "true")
            suspend();

        stopTimer();
    });
    timer->start(getTime() * 1000);
}

void ScreenManager::stopTimer()
{
    delete timer;
    timer = nullptr;
}

void ScreenManager::wakeUp()
{
    if(!is_suspended) return;
    emit wakeUpScreenStart();
}

void ScreenManager::wakeUpNow()
{
    if (!is_suspended) return;
    is_suspended = false;

    //Sometimes the screen wakes up but it remain black
    //doing ON->OFF->ON prevents this
    XUtils::WakeUpScreen(true);
    XUtils::WakeUpScreen(false);
    XUtils::WakeUpScreen(true);

    startTimer();
    emit wakeUpScreen();

    XUtils::UpdateDPMS(false, 0);
}

void ScreenManager::wakeUpNowWhenScreenOn()
{
    if (!is_suspended) return;
    is_suspended = false;

    startTimer();
    emit wakeUpScreen();
}

void ScreenManager::suspendNow()
{
    if (is_suspended) return;
    is_suspended = true;

    XUtils::UpdateDPMS(true, 0);
    XUtils::WakeUpScreen(false);

    stopTimer();
    emit suspendScreen();
}

void ScreenManager::suspend()
{
    if (is_suspended) return;
    emit suspendScreenStart();
}
