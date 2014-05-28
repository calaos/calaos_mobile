#ifndef COMMON_H
#define COMMON_H

#include <QObject>
#include <QtQml>

class Common : public QObject
{
    Q_OBJECT
public:
    enum AppStatus {
        NotConnected,
        Loading,
        LoggedIn
    };
    Q_ENUMS(AppStatus)

    static void registerQml()
    {
        qmlRegisterType<Common>("Calaos", 1, 0, "Common");
    }
};

#endif // COMMON_H
