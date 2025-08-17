#ifndef COMMON_H
#define COMMON_H

#include <QObject>
#include <QtCore>
#include <QtQml>

class Common : public QObject
{
    Q_OBJECT
public:

    static void installMessageOutputHandler();

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
        CameraOutput,
        FavoritesLightsCount,

        //Special type for styled items
        //they must be different as Light are treated differently with LightOnModel
        Pump,
        Outlet,
        Heater,
        Boiler,

        //Special style for binary switches
        DoorSensor,
        OccupancySensor,
        SmokeSensor,
        WaterLeakSensor,
        GasLeakSensor,
        CO2Sensor,
        SoundSensor,
        MotionSensor,
        VibrationSensor,
        LockSensor,
        GarageDoorSensor,
    };
    Q_ENUMS(IOType)

    enum FavoriteType {
        FavIO = 0,
        FavLightsCount,
        FavShutterCount,
        FavAudio,
        FavCamera
    };
    Q_ENUMS(FavoriteType)

    enum AudioStatusType {
        StatusUnknown = 0,
        StatusPlay,
        StatusPause,
        StatusStop
    };
    Q_ENUMS(AudioStatusType)

    enum FilterModelType {
        FilterAll = 0,
        FilterLeft,
        FilterRight,
        FilterScenario,
    };
    Q_ENUMS(FilterModelType)

    enum EventType {
        EventUnknown = 0,
        EventIoChanged = 3,
        EventPush = 22,
    };
    Q_ENUMS(EventType)

    static QString IOTypeToString(IOType t);
    static IOType IOTypeFromString(QString t, QString style = {});
    static QString audioStatusToString(AudioStatusType t);
    static AudioStatusType audioStatusFromString(QString t);

    static void registerQml()
    {
        qmlRegisterType<Common>("Calaos", 1, 0, "Common");
    }

    //Used for our demo
    static QString getDemoUser();
    static QString getDemoPass();
    static QString getDemoHost();
};

#endif // COMMON_H
