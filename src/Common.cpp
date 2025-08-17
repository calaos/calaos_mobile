#include "Common.h"
#include <QStandardPaths>

QString Common::IOTypeToString(Common::IOType t)
{
    switch (t)
    {
    case Outlet:
    case Pump:
    case Boiler:
    case Heater:
    case Light: return "light";
    case Temp: return "temp";
    case AnalogIn: return "analog_in";
    case AnalogOut: return "analog_out";
    case LightDimmer: return "light_dimmer";
    case LightRgb: return "light_rgb";
    case Shutter: return "shutter";
    case ShutterSmart: return "shutter_smart";
    case VarBool: return "var_bool";
    case VarInt: return "var_int";
    case VarString: return "var_string";
    case Scenario: return "scenario";
    case AVReceiver: return "avreceiver";
    case StringIn: return "string_in";
    case StringOut: return "string_out";
    case Timer: return "timer";
    case Time: return "time";
    case TimeRange: return "time_range";
    case Switch: return "switch";
    case Switch3: return "switch3";
    case SwitchLong: return "switch_long";
    case AudioInput: return "audio_input";
    case AudioOutput: return "audio_output";
    case CameraInput: return "camera_input";
    case CameraOutput: return "camera_output";
    case FavoritesLightsCount: return "fav_all_lights";
    default: break;
    }

    return QString();
}

Common::IOType Common::IOTypeFromString(QString t, QString style)
{
    if (t == "light" && style == "pump") return Pump;
    if (t == "light" && style == "outlet") return Outlet;
    if (t == "light" && style == "boiler") return Boiler;
    if (t == "light" && style == "heater") return Heater;
    if (t == "light") return Light;
    if (t == "temp") return Temp;
    if (t == "analog_in") return AnalogIn;
    if (t == "analog_out") return AnalogOut;
    if (t == "light_dimmer") return LightDimmer;
    if (t == "light_rgb") return LightRgb;
    if (t == "shutter") return Shutter;
    if (t == "shutter_smart") return ShutterSmart;
    if (t == "var_bool") return VarBool;
    if (t == "var_int") return VarInt;
    if (t == "var_string") return VarString;
    if (t == "scenario") return Scenario;
    if (t == "avreceiver") return AVReceiver;
    if (t == "string_in") return StringIn;
    if (t == "string_out") return StringOut;
    if (t == "timer") return Timer;
    if (t == "time") return Time;
    if (t == "time_range") return TimeRange;
    if (t == "switch" && style == "door" ) return DoorSensor;
    if (t == "switch" && style == "occupancy" ) return OccupancySensor;
    if (t == "switch" && style == "smoke" ) return SmokeSensor;
    if (t == "switch" && style == "water" ) return WaterLeakSensor;
    if (t == "switch" && style == "gas" ) return GasLeakSensor;
    if (t == "switch" && style == "carbon_monoxide" ) return CO2Sensor;
    if (t == "switch" && style == "sound" ) return SoundSensor;
    if (t == "switch" && style == "motion" ) return MotionSensor;
    if (t == "switch" && style == "vibration" ) return VibrationSensor;
    if (t == "switch" && style == "lock" ) return LockSensor;
    if (t == "switch" && style == "garage_door" ) return GarageDoorSensor;
    if (t == "switch") return Switch;
    if (t == "switch3") return Switch3;
    if (t == "switch_long") return SwitchLong;
    if (t == "audio_input") return AudioInput;
    if (t == "audio_output") return AudioOutput;
    if (t == "camera_input") return CameraInput;
    if (t == "camera_output") return CameraOutput;
    if (t == "fav_all_lights") return FavoritesLightsCount;

    return Unkown;
}

QString Common::audioStatusToString(Common::AudioStatusType t)
{
    switch (t)
    {
    case StatusPause: return "pause";
    case StatusPlay: return "play";
    case StatusStop: return "stop";
    default: break;
    }

    return QString();
}

Common::AudioStatusType Common::audioStatusFromString(QString t)
{
    if (t == "play" || t == "playing") return StatusPlay;
    if (t == "pause") return StatusPause;
    if (t == "stop") return StatusStop;

    return StatusUnknown;
}

static void _messageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString fname = context.file;
    fname = fname.section('\\', -1, -1);

    switch (type) {
    default:
    case QtDebugMsg:
    {
        QString s = QString("DEBUG: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtWarningMsg:
    {
        QString s = QString("WARNING: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtCriticalMsg:
    {
        QString s = QString("CRITICAL: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    case QtFatalMsg:
    {
        QString s = QString("FATAL: %1:%2 - %3\n").arg(fname).arg(context.line).arg(msg);
        printf("%s", qPrintable(s));
        break;
    }
    }

    fflush(stdout);
}

void Common::installMessageOutputHandler()
{
    qInstallMessageHandler(_messageOutput);
}

QString Common::getDemoUser()
{
    return QStringLiteral("demo@calaos.fr");
}

QString Common::getDemoPass()
{
    return QStringLiteral("demo");
}

QString Common::getDemoHost()
{
    return QStringLiteral("demo.calaos.fr");
}
