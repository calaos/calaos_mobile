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

private:
    ScreenManager(QObject *parent = 0);
};

#endif // SCREENMANAGER_H
