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
    case FavoritesLightsCount: return "fav_all_lights";
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

//This function is used to completely clean a string from
//any accent, diacritic marks, ligature letter without diacritics, ...
//It will then be used to search for the best matching string in the Rooms names/IO names
QString Common::removeSpecialChar(const QString &s)
{
    static QString diacriticLetters = QString::fromUtf8("ŠŒŽšœžŸ¥µÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝßàáâãäåæçèéêëìíîïðñòóôõöøùúûüýÿ");
    static QStringList noDiacriticLetters = QStringList() << "S" << "OE" << "Z" << "s" << "oe" << "z" << "Y" << "Y" << "u"
                                                          << "A" << "A" << "A" << "A" << "A" << "A" << "AE" << "C" << "E"
                                                          << "E" << "E" << "E" << "I" << "I" << "I" << "I" << "D" << "N"
                                                          << "O" << "O" << "O" << "O" << "O" << "O" << "U" << "U" << "U"
                                                          << "U" << "Y" << "s" << "a" << "a" << "a" << "a" << "a" << "a"
                                                          << "ae" << "c" << "e" << "e" << "e" << "e" << "i" << "i" << "i"
                                                          << "i" << "o" << "n" << "o" << "o" << "o" << "o" << "o" << "o"
                                                          << "u" << "u" << "u" << "u" << "y" << "y";

    //Replace the special characters from our list
    QString output = "";
    for (int i = 0; i < s.length(); i++)
    {
        QChar c = s[i];
        int dIndex = diacriticLetters.indexOf(c);
        if (dIndex < 0)
            output.append(c);
        else
        {
            QString replacement = noDiacriticLetters[dIndex];
            output.append(replacement);
        }
    }

    //Decompose string if some chars were forgotten
    QString out(output.normalized(QString::NormalizationForm_KD));

    //return only ascii chars, no punctuation marks
    return out.replace(QRegExp("[^a-zA-Z\\s]"), "").toLower();
}
