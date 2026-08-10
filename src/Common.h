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

    /* Defensive parsing of numeric values coming from the server (or from a
     * config file). QString::toInt() / QVariant::toDouble() without a success
     * flag silently turn malformed data into 0, which then feeds the business
     * logic as if it were a real value.
     *
     * Contract, identical for all three overloads:
     *  - readable value      -> the converted value (behaviour unchanged);
     *  - missing/empty field -> `def`, silently. A key missing from a
     *                           QVariantMap is normal, not corrupted data,
     *                           and warning on it would flood the logs;
     *  - present but
     *    unconvertible value -> `def` plus a qWarning naming the field.
     *
     * `what` only makes the warning actionable: pass the field name as it
     * appears in the protocol ("IOBase.state", ...).
     * The default is 0 everywhere it is used in src/, so replacing a bare parse
     * with these helpers changes no value: it only adds the trace. */
    static int toIntSafe(const QString &s, int def = 0, const char *what = nullptr);
    static double toDoubleSafe(const QString &s, double def = 0.0, const char *what = nullptr);
    static qint64 toLongLongSafe(const QString &s, qint64 def = 0, const char *what = nullptr);

    static int toIntSafe(const QVariant &v, int def = 0, const char *what = nullptr);
    static double toDoubleSafe(const QVariant &v, double def = 0.0, const char *what = nullptr);
    static qint64 toLongLongSafe(const QVariant &v, qint64 def = 0, const char *what = nullptr);

    //Handle both a native JSON number and a number carried as a string.
    static int toIntSafe(const QJsonValue &v, int def = 0, const char *what = nullptr);
    static double toDoubleSafe(const QJsonValue &v, double def = 0.0, const char *what = nullptr);

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
