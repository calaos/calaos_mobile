#ifndef SCREENMANAGER_H
#define SCREENMANAGER_H

#include <QtCore>

class ScreenManager: public QObject
{
    Q_OBJECT
public:
    virtual ~ScreenManager();

    static ScreenManager& Instance()
    {
        static ScreenManager s;
        return s;
    }

    // Wake up when the screen is on, avoid to flash the screen */
    Q_INVOKABLE void wakeUpNowWhenScreenOn();

    // Re-init the timer to 0, if the timer reach the time returns by getTime(), the screen will be suspend
    Q_INVOKABLE void updateTimer();

    Q_INVOKABLE int getTime();

    Q_INVOKABLE void startTimer();
    Q_INVOKABLE void stopTimer();
    Q_INVOKABLE void wakeUp();
    Q_INVOKABLE void suspend();

    Q_INVOKABLE void wakeUpNow();
    Q_INVOKABLE void suspendNow();

signals:
    void wakeUpScreenStart();
    void wakeUpScreen();
    void suspendScreenStart();
    void suspendScreen();

private:
    QTimer *timer = nullptr;
    bool is_suspended = false;

    ScreenManager(QObject *parent = 0);
};

#endif // SCREENMANAGER_H
