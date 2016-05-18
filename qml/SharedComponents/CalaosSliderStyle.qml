import QtQuick 2.4
import QtQuick.Controls.Styles 1.4
import "."

SliderStyle {
    id: style

    property color color: control.hasOwnProperty("color")
            ? control.color : "#3AB4D7"

    property Component customBackground: null

    groove: Item {
        implicitWidth: 200 * calaosApp.density
        implicitHeight: 2 * calaosApp.density

        Rectangle {
            visible: customBackground === null

            anchors.fill: parent
            anchors.verticalCenter: parent.verticalCenter

            color: Qt.rgba(255, 255, 255, 0.3)

            Rectangle {
                height: parent.height
                width: styleData.handlePosition
                implicitHeight: 2 * calaosApp.density
                implicitWidth: 200 * calaosApp.density
                color: style.color
            }
        }

        Loader {
            visible: customBackground !== null
            anchors.fill: parent
            sourceComponent: customBackground
        }
    }

    function colorAlpha(c, alpha) {
        var realColor = Qt.darker(color, 1)
        realColor.a = alpha
        return realColor
    }

    handle: Item {
        anchors.centerIn: parent
        implicitHeight: 8 * calaosApp.density
        implicitWidth: 8 * calaosApp.density

        Rectangle {
            anchors.centerIn: parent
            implicitHeight: 32 * calaosApp.density
            implicitWidth: 32 * calaosApp.density
            color: control.focus ?
                       colorAlpha(style.color, 0.20) : "transparent"
            radius: implicitHeight / 2

            Rectangle {
                property var diameter: control.enabled ? 18 * calaosApp.density:
                                                         12 * calaosApp.density
                anchors.centerIn: parent
                color: control.value === control.minimumValue ?
                           "#e7e7e7" : style.color

                border.color: control.value === control.minimumValue?
                              style.darkBackground? colorAlpha("#FFFFFF", 0.3):
                                                    colorAlpha("#000000", 0.26):
                                                    style.color

                border.width: 2 * calaosApp.density

                implicitHeight: control.pressed && !control.focus ?
                                    diameter + 8 * calaosApp.density :
                                    diameter

                implicitWidth: control.pressed && !control.focus ?
                                   diameter + 8 * calaosApp.density :
                                   diameter

                radius: implicitWidth / 2

                Behavior on implicitHeight { NumberAnimation { duration: 200} }
                Behavior on implicitWidth { NumberAnimation { duration: 200} }
            }
        }
    }
}
