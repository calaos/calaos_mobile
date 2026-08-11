import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Universal
//import QtQuick.Controls.Styles 1.2
import SharedComponents

ButtonStyle {
    background: Item {
        property bool down: control.pressed || (control.checkable && control.checked)
        BorderImage {
            source: calaosApp.getPictureSized("back_items_home")
            border.left: Units.dp(5); border.top: Units.dp(5)
            border.right: Units.dp(5); border.bottom: Units.dp(5)
            anchors.fill: parent
            opacity: down?0:1
            Behavior on opacity { PropertyAnimation { duration: 50 } }
        }
        BorderImage {
            source: calaosApp.getPictureSized("back_items_home_glow")
            border.left: Units.dp(5); border.top: Units.dp(5)
            border.right: Units.dp(5); border.bottom: Units.dp(5)
            anchors.fill: parent
            opacity: down?1:0
            Behavior on opacity { PropertyAnimation { duration: 50 } }
        }
    }
    padding {
            top: Units.dp(12)
            left: Units.dp(12)
            right: Units.dp(12)
            bottom: Units.dp(12)
    }
    label: Text {
        text: control.text
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        font { family: calaosFont.fontFamily; pointSize: 12 }
        color: "#e7e7e7"
        anchors.fill: parent
    }
}
