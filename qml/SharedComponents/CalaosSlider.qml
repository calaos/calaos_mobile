import QtQuick
import QtQuick.Controls as Controls
import "."

Controls.Slider {
    id: control

    implicitHeight: orientation === Qt.Vertical? Units.dp(200): Units.dp(40)
    implicitWidth: orientation === Qt.Vertical? Units.dp(40): Units.dp(200)

    property Component customBackground: null

    property color color: "#3AB4D7"

    background: Item {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: Units.dp(200)
        implicitHeight: Units.dp(4)
        width: control.availableWidth
        height: implicitHeight

        Rectangle {
            visible: customBackground? false: true

            anchors.fill: parent

            radius: 2
            color: Qt.rgba(255, 255, 255, 0.3)

            Rectangle {
                width: control.visualPosition * parent.width
                height: parent.height
                color: control.color
                radius: 2
            }
        }

        Loader {
            visible: customBackground? true: false
            anchors.fill: parent
            sourceComponent: customBackground
        }
    }

    handle: Item {
        implicitHeight: Units.dp(8)
        implicitWidth: Units.dp(8)

        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2

        Rectangle {
            anchors.centerIn: parent
            implicitHeight: Units.dp(32)
            implicitWidth: Units.dp(32)
            color: control.focus ?
                       Theme.colorAlpha(control.color, 0.20) : "transparent"
            radius: implicitHeight / 2

            Rectangle {
                property var diameter: control.enabled ? Units.dp(18):
                                                         Units.dp(12)
                anchors.centerIn: parent
                color: control.value === control.minimumValue ?
                           "#e7e7e7" : control.color

                border.color: control.value === control.from? Theme.colorAlpha("#FFFFFF", 0.3): control.color

                border.width: Units.dp(2)

                implicitHeight: control.pressed && !control.focus ?
                                    diameter + Units.dp(8) :
                                    diameter

                implicitWidth: control.pressed && !control.focus ?
                                   diameter + Units.dp(8) :
                                   diameter

                radius: implicitWidth / 2

                Behavior on implicitHeight { NumberAnimation { duration: 200} }
                Behavior on implicitWidth { NumberAnimation { duration: 200} }
            }
        }
    }

}
