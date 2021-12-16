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

    Component {
        id: delegate

        Loader {
            z: index
            sourceComponent: model.ioType === Common.Pump? pump:
                             model.ioType === Common.Outlet? outlet:
                             model.ioType === Common.Boiler? boiler:
                             model.ioType === Common.Heater? heater:
                             model.ioType === Common.Light? light:
                             model.ioType === Common.Temp? temp:
                             model.ioType === Common.VarInt? var_int:
                             model.ioType === Common.AnalogIn && (model.ioStyle !== "default" && model.ioStyle !== "") ? analogStyled:
                             model.ioType === Common.AnalogIn? var_int:
                             model.ioType === Common.AnalogOut? var_int:
                             model.ioType === Common.VarBool? var_bool:
                             model.ioType === Common.VarString? var_string:
                             model.ioType === Common.StringIn? var_string:
                             model.ioType === Common.StringOut? var_string:
                             model.ioType === Common.Scenario? scenario:
                             model.ioType === Common.Shutter? shutter:
                             model.ioType === Common.LightDimmer? light_dimmer:
                             model.ioType === Common.LightRgb? light_rgb:
                             model.ioType === Common.ShutterSmart? shutter_smart:
                             model.ioType === Common.FavoritesLightsCount? fav_all_lights:
                             default_delegate

            onLoaded: {
                item.modelData = Qt.binding(function() { return lst.model.getItemModel(model.index) })
                console.debug("model is: " + model.roomName)
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
}
