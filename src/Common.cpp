#include "Common.h"

QString Common::IOTypeToString(Common::IOType t)
{
    switch (t)
    {
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
    default: break;
    }

    return QString();
}

Common::IOType Common::IOTypeFromString(QString t)
{
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
    if (t == "switch") return Switch;
    if (t == "switch3") return Switch3;
    if (t == "switch_long") return SwitchLong;
    if (t == "audio_input") return AudioInput;
    if (t == "audio_output") return AudioOutput;
    if (t == "camera_input") return CameraInput;
    if (t == "camera_output") return CameraOutput;

    return Unkown;
}
