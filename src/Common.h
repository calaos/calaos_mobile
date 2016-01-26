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
        FavoritesLightsCount
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

    static QString IOTypeToString(IOType t);
    static IOType IOTypeFromString(QString t);
    static QString audioStatusToString(AudioStatusType t);
    static AudioStatusType audioStatusFromString(QString t);

    static void registerQml()
    {
        qmlRegisterType<Common>("Calaos", 1, 0, "Common");
    }
};

#endif // COMMON_H
