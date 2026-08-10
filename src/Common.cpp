#include "Common.h"
#include <QStandardPaths>
#include <cmath>
#include <limits>

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

/* Single trace shared by every parsing helper: only one format to know when
 * grepping the logs for "which server value was unreadable". */
static void warnParseFailed(const char *kind, const char *what, const QString &raw)
{
    qWarning().noquote() << QStringLiteral("Common: cannot parse %1 as %2, got \"%3\", using default")
                            .arg(QString::fromLatin1(what ? what : "value"),
                                 QString::fromLatin1(kind),
                                 raw);
}

/* A key missing from a QVariantMap, or a field the server sent empty, is not
 * malformed data: return the default without polluting the logs. */
static bool isAbsentOrEmpty(const QVariant &v)
{
    if (!v.isValid() || v.isNull())
        return true;

    const int id = v.metaType().id();
    if (id == QMetaType::QString || id == QMetaType::QByteArray)
        return v.toString().isEmpty();

    return false;
}

int Common::toIntSafe(const QString &s, int def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const int val = s.toInt(&ok);
    if (ok)
        return val;

    warnParseFailed("int", what, s);
    return def;
}

double Common::toDoubleSafe(const QString &s, double def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const double val = s.toDouble(&ok);
    if (ok)
        return val;

    warnParseFailed("double", what, s);
    return def;
}

qint64 Common::toLongLongSafe(const QString &s, qint64 def, const char *what)
{
    if (s.isEmpty())
        return def;

    bool ok = false;
    const qint64 val = s.toLongLong(&ok);
    if (ok)
        return val;

    warnParseFailed("qint64", what, s);
    return def;
}

int Common::toIntSafe(const QVariant &v, int def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const int val = v.toInt(&ok);
    if (ok)
        return val;

    warnParseFailed("int", what, v.toString());
    return def;
}

double Common::toDoubleSafe(const QVariant &v, double def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const double val = v.toDouble(&ok);
    if (ok)
        return val;

    warnParseFailed("double", what, v.toString());
    return def;
}

qint64 Common::toLongLongSafe(const QVariant &v, qint64 def, const char *what)
{
    if (isAbsentOrEmpty(v))
        return def;

    bool ok = false;
    const qint64 val = v.toLongLong(&ok);
    if (ok)
        return val;

    warnParseFailed("qint64", what, v.toString());
    return def;
}

int Common::toIntSafe(const QJsonValue &v, int def, const char *what)
{
    if (v.isUndefined() || v.isNull())
        return def;

    if (v.isDouble())
    {
        const double d = v.toDouble();
        if (std::trunc(d) == d &&
            d >= static_cast<double>(std::numeric_limits<int>::min()) &&
            d <= static_cast<double>(std::numeric_limits<int>::max()))
            return static_cast<int>(d);

        warnParseFailed("int", what, QString::number(d));
        return def;
    }

    //some servers carry numbers as strings
    if (v.isString())
        return toIntSafe(v.toString(), def, what);

    warnParseFailed("int", what, v.toVariant().toString());
    return def;
}

double Common::toDoubleSafe(const QJsonValue &v, double def, const char *what)
{
    if (v.isUndefined() || v.isNull())
        return def;

    if (v.isDouble())
        return v.toDouble();

    if (v.isString())
        return toDoubleSafe(v.toString(), def, what);

    warnParseFailed("double", what, v.toVariant().toString());
    return def;
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
