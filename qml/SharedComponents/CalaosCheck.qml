import QtQuick
import QtQuick.Controls as Controls
import "."

Controls.CheckBox {
    id: control

    implicitHeight: Units.dp(40)
    implicitWidth: Units.dp(200)

    background: Rectangle {
        color: "transparent"
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        font.weight: Font.Thin
        color: "#E7E7E7"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }

    indicator: Item {
        implicitWidth: Units.dp(48)
        implicitHeight: Units.dp(48)
        x: control.leftPadding
        y: parent.height / 2 - height / 2

        Rectangle {
            anchors.centerIn: parent
            implicitWidth: Units.dp(30)
            implicitHeight: Units.dp(30)
            color: control.activeFocus ? Theme.colorAlpha(Theme.whiteColor, 0.34) : "transparent"
        }

        Rectangle {
            anchors.centerIn: parent

            implicitWidth: Units.dp(20)
            implicitHeight: Units.dp(20)
            color: "transparent"

            border.color: control.enabled
                ? control.checked ? "#3AB4D7"
                                  : Theme.colorAlpha(Theme.whiteColor, 0.54)
                : Theme.colorAlpha(Theme.whiteColor, 0.26)

            border.width: Units.dp(2)
            antialiasing: true

            Behavior on border.color {
                ColorAnimation { duration: 200}
            }

            Rectangle {
                anchors {
                    centerIn: parent
                    alignWhenCentered: false
                }
                implicitWidth: control.checked ? Units.dp(10) : 0
                implicitHeight: control.checked ? Units.dp(10) : 0
                color: control.enabled ? "#3AB4D7"
                                       : Theme.colorAlpha(Theme.whiteColor, 0.26)
                antialiasing: true

                Behavior on implicitWidth {
                    NumberAnimation { duration: 200 }
                }

                Behavior on implicitHeight {
                    NumberAnimation { duration: 200 }
                }
            }
        }
    }
}
