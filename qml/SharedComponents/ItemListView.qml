import QtQuick
import Calaos
import "."

ListView {
    id: lst

    property bool showHeader: true

    Component {
        id: sectionHeading
        Rectangle {
            z: 99
            width: lst.width
            color: "#080808"
            height: 45 * calaosApp.density

            Image {
                id: ic
                source: calaosApp.getPictureSized("icon_room")
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left; leftMargin: 10 * calaosApp.density
                }
            }

            Text {
                id: txt
                color: "#e7e7e7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: section
                anchors {
                    left: ic.source === ""?parent.left:ic.right
                    leftMargin: 5 * calaosApp.density
                    verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                color: "#333333"
                height: 2 * calaosApp.density
            }
        }
    }

    section.property: showHeader?"roomName":""
    section.criteria: ViewSection.FullString
    section.delegate: showHeader?sectionHeading:null
    section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart

    width: parent.width
    height: parent.height

    spacing: 10 * calaosApp.density

    delegate: delegate

    //One delegate per Common.IOType, listed in the order src/Common.h declares
    //the enum so the two can be read side by side. A type absent from the table
    //falls back to default_delegate (a plain name label), which is what the
    //ternary chain this replaces did once it ran out of branches.
    //src/IOTypeRegistry is the C++ side table for the same enum: a type added
    //there needs a row here too, or it silently shows up as a bare label.
    readonly property var delegateByType: ({
        //Unkown: default_delegate
        [Common.Light]: light,
        [Common.Temp]: temp,
        //AnalogIn also has a styled variant, see delegateForItem()
        [Common.AnalogIn]: var_int,
        [Common.AnalogOut]: var_int,
        [Common.LightDimmer]: light_dimmer,
        [Common.LightRgb]: light_rgb,
        [Common.Shutter]: shutter,
        [Common.ShutterSmart]: shutter_smart,
        [Common.VarBool]: var_bool,
        [Common.VarInt]: var_int,
        [Common.VarString]: var_string,
        [Common.Scenario]: scenario,
        //AVReceiver: default_delegate, driven from the media views
        [Common.StringIn]: var_string,
        [Common.StringOut]: var_string,
        //Timer, Time, TimeRange: default_delegate, no widget for them yet
        [Common.Switch]: ioswitch,
        [Common.Switch3]: ioswitch,
        [Common.SwitchLong]: ioswitch,
        //AudioInput, AudioOutput, CameraInput, CameraOutput: default_delegate,
        //they have their own views
        [Common.FavoritesLightsCount]: fav_all_lights,

        //styled binary devices
        [Common.Pump]: pump,
        [Common.Outlet]: outlet,
        [Common.Heater]: heater,
        [Common.Boiler]: boiler,

        //binary sensors, all drawn by IOSwitch which picks its own icon and
        //wording from the type
        [Common.DoorSensor]: ioswitch,
        [Common.OccupancySensor]: ioswitch,
        [Common.SmokeSensor]: ioswitch,
        [Common.WaterLeakSensor]: ioswitch,
        [Common.GasLeakSensor]: ioswitch,
        [Common.CO2Sensor]: ioswitch,
        [Common.SoundSensor]: ioswitch,
        [Common.MotionSensor]: ioswitch,
        [Common.VibrationSensor]: ioswitch,
        [Common.LockSensor]: ioswitch,
        [Common.GarageDoorSensor]: ioswitch
    })

    //AnalogIn is the only type whose delegate also depends on the style the
    //server sent: a styled gauge when there is one, the plain numeric widget
    //otherwise. Every other type is decided by delegateByType alone.
    function delegateForItem(ioType, ioStyle) {
        if (ioType === Common.AnalogIn && ioStyle !== "default" && ioStyle !== "")
            return analogStyled

        var comp = delegateByType[ioType]
        return comp !== undefined? comp: default_delegate
    }

    Component {
        id: delegate

        Loader {
            z: index
            sourceComponent: lst.delegateForItem(model.ioType, model.ioStyle)

            onLoaded: {
                item.modelData = Qt.binding(function() { return lst.model.getItemModel(model.index) })
            }

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: -3 * calaosApp.density
            width: parent.width - 10 * calaosApp.density
        }
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: calaosApp.getPictureSized("back_items_home")
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            width: parent.width
            height: 40 * calaosApp.density

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: modelData.ioName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: parent.left; leftMargin: 8 * calaosApp.density
                    right: parent.right; rightMargin: 8 * calaosApp.density
                    verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Component { id: light; IOLight {} }
    Component { id: temp; IOTemp {} }
    Component { id: var_int; IOVarInt {} }
    Component { id: var_bool; IOVarBool {} }
    Component { id: var_string; IOVarString {} }
    Component { id: scenario; IOScenario {} }
    Component { id: shutter; IOShutter {} }
    Component { id: light_dimmer; IOLightDimmer {} }
    Component { id: light_rgb; IOLightRGB {} }
    Component { id: shutter_smart; IOShutterSmart {} }
    Component { id: fav_all_lights; IOFavAllLights {} }
    Component { id: pump; IOPump {} }
    Component { id: outlet; IOOutlet {} }
    Component { id: boiler; IOBoiler {} }
    Component { id: heater; IOHeater {} }
    Component { id: analogStyled; IOAnalogStyled {} }
    Component { id: ioswitch; IOSwitch {} }
}
