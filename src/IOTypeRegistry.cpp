#include "IOTypeRegistry.h"

#include <QHash>
#include <QStringList>

namespace
{

using Category = IOTypeRegistry::Category;
using Entry = IOTypeRegistry::Entry;

/* The one table. Columns, in order:
 *   IOType | gui_type | io_style | category | isLight | dimmable |
 *   visible as a room input | visible as a room output
 *
 * Common::Unkown deliberately has no row: it is the answer given when nothing
 * matches, not a type the server can send.
 */
const QVector<Entry> &table()
{
    static const QVector<Entry> t = {
        //Lights. The four styled ones travel as gui_type "light" and render
        //like a light, but they are appliances: countsAsLight is false for
        //them, so the light counter and "all lights off" leave them alone.
        { Common::Light,                "light",         "",                Category::Light,    true,  true,  false, false, true  },
        { Common::Pump,                 "light",         "pump",            Category::Light,    true, false,  false, false, true  },
        { Common::Outlet,               "light",         "outlet",          Category::Light,    true, false,  false, false, true  },
        { Common::Boiler,               "light",         "boiler",          Category::Light,    true, false,  false, false, true  },
        { Common::Heater,               "light",         "heater",          Category::Light,    true, false,  false, false, true  },
        { Common::LightDimmer,          "light_dimmer",  "",                Category::Light,    true,  true,  true,  false, true  },
        { Common::LightRgb,             "light_rgb",     "",                Category::Light,    true,  true,  true,  false, true  },

        { Common::Shutter,              "shutter",       "",                Category::Shutter,  false, false, false, false, true  },
        { Common::ShutterSmart,         "shutter_smart", "",                Category::Shutter,  false, false, false, false, true  },

        { Common::Temp,                 "temp",          "",                Category::Temp,     false, false, false, true,  false },
        { Common::AnalogIn,             "analog_in",     "",                Category::Temp,     false, false, false, true,  false },
        { Common::AnalogOut,            "analog_out",    "",                Category::Other,    false, false, false, false, true  },

        { Common::VarBool,              "var_bool",      "",                Category::Var,      false, false, false, false, true  },
        { Common::VarInt,               "var_int",       "",                Category::Var,      false, false, false, false, true  },
        { Common::VarString,            "var_string",    "",                Category::Var,      false, false, false, false, true  },

        { Common::Scenario,             "scenario",      "",                Category::Scenario, false, false, false, true,  false },

        { Common::AVReceiver,           "avreceiver",    "",                Category::Media,    false, false, false, false, false },
        { Common::AudioInput,           "audio_input",   "",                Category::Media,    false, false, false, false, false },
        { Common::AudioOutput,          "audio_output",  "",                Category::Media,    false, false, false, false, false },
        { Common::CameraInput,          "camera_input",  "",                Category::Media,    false, false, false, false, false },
        { Common::CameraOutput,         "camera_output", "",                Category::Media,    false, false, false, false, false },

        { Common::StringIn,             "string_in",     "",                Category::Other,    false, false, false, true,  false },
        { Common::StringOut,            "string_out",    "",                Category::Other,    false, false, false, false, true  },

        { Common::Timer,                "timer",         "",                Category::Other,    false, false, false, false, false },
        { Common::Time,                 "time",          "",                Category::Other,    false, false, false, false, false },
        { Common::TimeRange,            "time_range",    "",                Category::Other,    false, false, false, false, false },

        //Switches. The styled ones are binary sensors, sent as gui_type
        //"switch" like a plain switch and shown in a room the same way.
        { Common::Switch,               "switch",        "",                Category::Other,    false, false, false, true,  false },
        { Common::DoorSensor,           "switch",        "door",            Category::Sensor,   false, false, false, true,  false },
        { Common::OccupancySensor,      "switch",        "occupancy",       Category::Sensor,   false, false, false, true,  false },
        { Common::SmokeSensor,          "switch",        "smoke",           Category::Sensor,   false, false, false, true,  false },
        { Common::WaterLeakSensor,      "switch",        "water",           Category::Sensor,   false, false, false, true,  false },
        { Common::GasLeakSensor,        "switch",        "gas",             Category::Sensor,   false, false, false, true,  false },
        { Common::CO2Sensor,            "switch",        "carbon_monoxide", Category::Sensor,   false, false, false, true,  false },
        { Common::SoundSensor,          "switch",        "sound",           Category::Sensor,   false, false, false, true,  false },
        { Common::MotionSensor,         "switch",        "motion",          Category::Sensor,   false, false, false, true,  false },
        { Common::VibrationSensor,      "switch",        "vibration",       Category::Sensor,   false, false, false, true,  false },
        { Common::LockSensor,           "switch",        "lock",            Category::Sensor,   false, false, false, true,  false },
        { Common::GarageDoorSensor,     "switch",        "garage_door",     Category::Sensor,   false, false, false, true,  false },
        { Common::Switch3,              "switch3",       "",                Category::Other,    false, false, false, false, false },
        { Common::SwitchLong,           "switch_long",   "",                Category::Other,    false, false, false, false, false },

        { Common::FavoritesLightsCount, "fav_all_lights","",                Category::Other,    false, false, false, false, false },
    };

    return t;
}

const QHash<int, const Entry *> &byIoType()
{
    static const QHash<int, const Entry *> h = []
    {
        QHash<int, const Entry *> m;
        for (const Entry &e: table())
            m.insert(static_cast<int>(e.ioType), &e);
        return m;
    }();

    return h;
}

/* (gui_type, io_style) -> row. The unstyled row of a gui_type is stored under
 * an empty style, which is exactly what a lookup with an unknown style falls
 * back to. */
QString pairKey(const QString &guiType, const QString &style)
{
    return guiType + QLatin1Char('\n') + style;
}

const QHash<QString, const Entry *> &byPair()
{
    static const QHash<QString, const Entry *> h = []
    {
        QHash<QString, const Entry *> m;
        for (const Entry &e: table())
            m.insert(pairKey(QString::fromLatin1(e.guiType), QString::fromLatin1(e.style)), &e);
        return m;
    }();

    return h;
}

/* Old protocol (no gui_type in the IO description): backend class name of an
 * IO -> gui_type it maps to. Kept verbatim from RoomModel::detectOldGuiType(),
 * which this replaces. */
const QHash<QString, QString> &legacyTable()
{
    static const QHash<QString, QString> h = {
        { "InputTime", "time" },
        { "InPlageHoraire", "time_range" },
        { "TimeRange", "time_range" },
        { "GpioInputSwitch", "switch" },
        { "GpioInputSwitchLongPress", "switch_long" },
        { "GpioInputSwitchTriple", "switch3" },
        { "OWTemp", "temp" },
        { "WIAnalog", "analog_in" },
        { "WagoInputAnalog", "analog_in" },
        { "WIDigitalBP", "switch" },
        { "WIDigital", "switch" },
        { "WagoInputSwitch", "switch" },
        { "WIDigitalLong", "switch_long" },
        { "WagoInputSwitchLongPress", "switch_long" },
        { "WIDigitalTriple", "switch3" },
        { "WagoInputSwitchTriple", "switch3" },
        { "WITemp", "temp" },
        { "WagoInputTemp", "temp" },
        { "WebInputSwitch", "switch" },
        { "WebInputAnalog", "analog_in" },
        { "WebInputTemp", "temp" },
        { "WebInputString", "string_in" },
        { "ZibaseTemp", "temp" },
        { "ZibaseAnalogIn", "analog_in" },
        { "ZibaseDigitalIn", "switch" },
        { "MySensorsInputAnalog", "analog_in" },
        { "MySensorsInputString", "string_in" },
        { "MySensorsInputSwitch", "switch" },
        { "MySensorsInputSwitchLongPress", "switch_long" },
        { "MySensorsInputSwitchTriple", "switch3" },
        { "MySensorsInputTemp", "temp" },
        { "PingInputSwitch", "switch" },
        { "KNXInputSwitch", "switch" },
        { "KNXInputAnalog", "analog_in" },
        { "KNXInputSwitchLongPress", "switch_long" },
        { "KNXInputSwitchTriple", "switch3" },
        { "KNXInputTemp", "temp" },
        { "OutputFake", "light" },
        { "GpioOutputSwitch", "light" },
        { "GpioOutputShutter", "shutter" },
        { "GpioOutputShutterSmart", "shutter_smart" },
        { "WOAnalog", "analog_out" },
        { "WagoOutputAnalog", "analog_out" },
        { "WODali", "light_dimmer" },
        { "WagoOutputDimmer", "light_dimmer" },
        { "WODaliRVB", "light_rgb" },
        { "WagoOutputDimmerRGB", "light_rgb" },
        { "WODigital", "light" },
        { "WagoOutputLight", "light" },
        { "WOVolet", "shutter" },
        { "WagoOutputShutter", "shutter" },
        { "WOVoletSmart", "shutter_smart" },
        { "WagoOutputShutterSmart", "shutter_smart" },
        { "X10Output", "light" },
        { "WebOutputString", "string_out" },
        { "WebOutputLight", "light" },
        { "WebOutputLightRGB", "light_rgb" },
        { "ZibaseDigitalOut", "light" },
        { "MySensorsOutputAnalog", "analog_out" },
        { "MySensorsOutputDimmer", "light_dimmer" },
        { "MySensorsOutputLight", "light" },
        { "MySensorsOutputLightRGB", "light_rgb" },
        { "MySensorsOutputShutter", "shutter" },
        { "MySensorsOutputShutterSmart", "shutter_smart" },
        { "MySensorsOutputString", "string_out" },
        { "OLAOutputLightDimmer", "light_dimmer" },
        { "OLAOutputLightRGB", "light_rgb" },
        { "WOLOutputBool", "var_bool" },
        { "KNXOutputLight", "light" },
        { "KNXOutputAnalog", "analog_out" },
        { "KNXOutputLightDimmer", "light_dimmer" },
        { "KNXOutputLightRGB", "light_rgb" },
        { "KNXOutputShutter", "shutter" },
        { "KNXOutputShutterSmart", "shutter_smart" },
        { "HueOutputLightRGB", "light_rgb" },
        { "InputTimer", "timer" },
        { "Scenario", "scenario" },
        { "InternalInt", "var_int" },
        { "InternalBool", "var_bool" },
        { "InternalString", "var_string" },
        { "AVReceiver", "avreceiver" },
        { "slim", "audio" },
        { "Squeezebox", "audio" },
        { "Axis", "camera" },
        { "Gadspot", "camera" },
        { "Planet", "camera" },
        { "StandardMjpeg", "camera" },
        { "standard_mjpeg", "camera" },
    };

    return h;
}

//Room media endpoints, matched on gui_type: two of them have no IOType.
const QStringList &mediaGuiTypes()
{
    static const QStringList l = {
        QStringLiteral("audio_output"),
        QStringLiteral("camera_output"),
        QStringLiteral("fav_all_lights"),
        QStringLiteral("audio_player"),
        QStringLiteral("camera"),
    };

    return l;
}

} //namespace

const QVector<IOTypeRegistry::Entry> &IOTypeRegistry::entries()
{
    return table();
}

const IOTypeRegistry::Entry *IOTypeRegistry::entry(Common::IOType t)
{
    return byIoType().value(static_cast<int>(t), nullptr);
}

Common::IOType IOTypeRegistry::fromGuiType(const QString &guiType, const QString &style)
{
    if (guiType.isEmpty())
        return Common::Unkown;

    if (!style.isEmpty())
    {
        const Entry *styled = byPair().value(pairKey(guiType, style), nullptr);
        if (styled)
            return styled->ioType;
    }

    const Entry *plain = byPair().value(pairKey(guiType, QString()), nullptr);
    if (plain)
        return plain->ioType;

    return Common::Unkown;
}

QString IOTypeRegistry::guiType(Common::IOType t)
{
    const Entry *e = entry(t);
    return e ? QString::fromLatin1(e->guiType) : QString();
}

QString IOTypeRegistry::style(Common::IOType t)
{
    const Entry *e = entry(t);
    return e ? QString::fromLatin1(e->style) : QString();
}

QString IOTypeRegistry::styleName(Common::IOType t)
{
    const Entry *e = entry(t);
    if (!e)
        return QString();

    return QString::fromLatin1(*e->style ? e->style : e->guiType);
}

IOTypeRegistry::Category IOTypeRegistry::category(Common::IOType t)
{
    const Entry *e = entry(t);
    return e ? e->category : Category::Other;
}

bool IOTypeRegistry::isLight(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->isLight;
}

bool IOTypeRegistry::isDimmableLight(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->isLight && e->dimmable;
}

bool IOTypeRegistry::isBinaryLight(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->isLight && !e->dimmable;
}

bool IOTypeRegistry::countsAsLight(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->countsAsLight;
}

bool IOTypeRegistry::isMeasurement(Common::IOType t)
{
    return category(t) == Category::Temp || t == Common::VarInt;
}

bool IOTypeRegistry::isRoomVisibleInput(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->roomVisibleInput;
}

bool IOTypeRegistry::isRoomVisibleOutput(Common::IOType t)
{
    const Entry *e = entry(t);
    return e && e->roomVisibleOutput;
}

bool IOTypeRegistry::isMediaGuiType(const QString &guiType)
{
    return mediaGuiTypes().contains(guiType);
}

QString IOTypeRegistry::legacyGuiType(const QString &protocolType)
{
    return legacyTable().value(protocolType);
}
