#ifndef COMMON_H
#define COMMON_H

#include <QObject>
#include <QtCore>
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

    enum MenuType {
        MenuNone,
        MenuMain,
        MenuBack
    };
    Q_ENUMS(MenuType)

    enum IOType {
        Unkown = 0,
        Light,
        Temp,
        AnalogIn,
        AnalogOut,
        LightDimmer,
        LightRgb,
        Shutter,
        ShutterSmart,
        VarBool,
        VarInt,
        VarString,
        Scenario,
        AVReceiver,
        StringIn,
        StringOut,
        Timer,
        Time,
        TimeRange,
        Switch,
        Switch3,
        SwitchLong,
        AudioInput,
        AudioOutput,
        CameraInput,
        CameraOutput
    };
    Q_ENUMS(IOType)

    static QString IOTypeToString(IOType t);
    static IOType IOTypeFromString(QString t);

    static void registerQml()
    {
        qmlRegisterType<Common>("Calaos", 1, 0, "Common");
    }
};

#endif // COMMON_H
