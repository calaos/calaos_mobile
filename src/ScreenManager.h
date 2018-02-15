#ifndef SCREENMANAGER_H
#define SCREENMANAGER_H

#include <QtCore>
#include "qqmlhelpers.h"

class ScreenManager: public QObject
{
    Q_OBJECT

    QML_WRITABLE_PROPERTY(int, dpmsTime)
    QML_WRITABLE_PROPERTY(bool, dpmsEnabled)
public:
    static ScreenManager& Instance()
    {
        static ScreenManager s;
        return s;
    }

    Q_INVOKABLE void suspendScreen();
    Q_INVOKABLE void wakeupScreen();

    Q_INVOKABLE void updateDpmsEnabled(bool en);
    Q_INVOKABLE void updateDpmsTime(int timeMin);

private:
    ScreenManager(QObject *parent = 0);

    void scheduleWriteConf();
    QTimer *writeConfTimer = nullptr;
};

#endif // SCREENMANAGER_H
