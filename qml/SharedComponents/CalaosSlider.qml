import QtQuick 2.4
import QtQuick.Controls as Controls
import "."

Controls.Slider {
    id: control

    implicitHeight: orientation === Qt.Vertical? Units.dp(200): Units.dp(40)
    implicitWidth: orientation === Qt.Vertical? Units.dp(40): Units.dp(200)

    property Component customBackground: undefined

    property color color: "#3AB4D7"

    background: Item {
        implicitWidth: 200 * calaosApp.density
        implicitHeight: 2 * calaosApp.density

        Rectangle {
            visible: customBackground? true: false

            anchors.fill: parent
            anchors.verticalCenter: parent.verticalCenter

            color: Qt.rgba(255, 255, 255, 0.3)

            Rectangle {
                height: parent.height
                width: control.visualPosition
                implicitHeight: 2 * calaosApp.density
                implicitWidth: 200 * calaosApp.density
                color: control.color
            }
        }

        Loader {
            visible: customBackground? true: false
            anchors.fill: parent
            sourceComponent: customBackground
        }
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
                       Theme.colorAlpha(style.color, 0.20) : "transparent"
            radius: implicitHeight / 2

            Rectangle {
                property var diameter: control.enabled ? 18 * calaosApp.density:
                                                         12 * calaosApp.density
                anchors.centerIn: parent
                color: control.value === control.minimumValue ?
                           "#e7e7e7" : style.color

                border.color: control.value === control.from?
                              style.darkBackground? Theme.colorAlpha("#FFFFFF", 0.3):
                                                    Theme.colorAlpha("#000000", 0.26):
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
