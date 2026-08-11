#ifndef IOTYPEREGISTRY_H
#define IOTYPEREGISTRY_H

#include <QString>
#include <QVector>

#include "Common.h"

/* Single source of truth for everything the app derives from the type of an IO.
 *
 * The server describes an IO along two independent axes: a gui_type
 * ("light", "switch", "shutter"...) and an optional io_style ("pump", "door"...).
 * Common::IOType is the flattening of that pair: Pump is (light, pump) and
 * DoorSensor is (switch, door). The table below is keyed by that pair, so:
 * - parsing (gui_type, io_style) and serializing back to a gui_type are two
 *   readings of the same row, and cannot drift apart;
 * - "is this a light?", "which family does it belong to?", "is it shown in a
 *   room?" are answered from that same row instead of being re-derived.
 *
 * Before this registry those lists were copy-pasted in Common, RoomModel,
 * RoomFilterModel and EventLogModel: forgetting a styled light (pump, outlet,
 * boiler, heater) in one of them silently dropped it from the light counter.
 * Adding an IO type is now a single new row here.
 */
class IOTypeRegistry
{
public:
    /* Coarse family an IO belongs to. It describes what the IO *is*; what a
     * given screen does with it stays in that screen. */
    enum class Category
    {
        Other = 0,
        Light,      //plain, styled and dimmable lights
        Shutter,
        Temp,       //analog readings (temperature, generic analog input)
        Sensor,     //styled binary sensors (door, smoke, motion...)
        Var,
        Scenario,
        Media,      //audio and camera endpoints
    };

    struct Entry
    {
        Common::IOType ioType;
        const char *guiType;
        const char *style;          //empty when the gui type alone identifies the IO
        Category category;
        bool isLight;               //feeds LightOnModel and the light counter
        bool dimmable;              //state is a level/color, not a boolean
        bool roomVisibleInput;      //listed in a room when read as an input
        bool roomVisibleOutput;     //listed in a room when read as an output
    };

    //Whole table, in declaration order. Unkown has no row.
    static const QVector<Entry> &entries();

    //Row of a type, or nullptr for Common::Unkown and any unlisted value.
    static const Entry *entry(Common::IOType t);

    /* (gui_type, io_style) -> IOType. An unknown style falls back to the
     * unstyled row of the same gui_type ("light" + "nope" is a plain Light),
     * an unknown gui_type gives Unkown. */
    static Common::IOType fromGuiType(const QString &guiType, const QString &style);

    //IOType -> gui_type ("light" for Pump, "switch" for DoorSensor).
    static QString guiType(Common::IOType t);

    //IOType -> io_style, empty for the unstyled types.
    static QString style(Common::IOType t);

    /* Name identifying the visual variant of a type: its style when it has
     * one, its gui_type otherwise ("light", "pump", "door"). Used to build
     * style dependent asset names. */
    static QString styleName(Common::IOType t);

    static Category category(Common::IOType t);

    //True for Light, the four styled lights, LightDimmer and LightRgb.
    static bool isLight(Common::IOType t);

    //Light whose state is a level or a color (LightDimmer, LightRgb).
    static bool isDimmableLight(Common::IOType t);

    //Light whose state is a boolean (Light and the four styled lights).
    static bool isBinaryLight(Common::IOType t);

    /* Numeric reading displayed as a value: temperature, analog input and
     * integer variable. Groups them on the same side of a room view. */
    static bool isMeasurement(Common::IOType t);

    static bool isRoomVisibleInput(Common::IOType t);
    static bool isRoomVisibleOutput(Common::IOType t);

    /* Media endpoints of a room are matched on their gui_type: some of them
     * ("audio_player", "camera") are legacy names with no IOType of their own. */
    static bool isMediaGuiType(const QString &guiType);

    /* Old protocol: IOs are described by their backend class name and carry no
     * gui_type. Maps that class name to the gui_type it would have today, or
     * an empty string when unknown. */
    static QString legacyGuiType(const QString &protocolType);
};

#endif // IOTYPEREGISTRY_H
