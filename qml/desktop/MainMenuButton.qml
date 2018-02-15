import QtQuick 2.2
import SharedComponents 1.0

Rectangle {

    id: bt

    property string iconName: "home"
    property bool selected: false
    property bool disabled: false
    property alias buttonLabel: buttonTxt.text

    signal clicked()

    width: Units.dp(142)
    height: Units.dp(94)

    color: "#303030"

    Image {
        source: "qrc:/img/button_%1.png".arg(iconName)
        anchors.fill: parent
        opacity: disabled?0:1.0
    }

    Image {
        source: "qrc:/img/button_menu_stop.png"
        anchors.fill: parent
        opacity: disabled?0.4:0.0
    }

    Image {
        source: "qrc:/img/button_menu_stop_on.png"
        anchors.fill: parent
        opacity: 0

        SequentialAnimation on opacity {
            id: stopAnim
            running: false
            NumberAnimation { from: 0; to: 1; duration: 22; easing.type: Easing.InCubic }
            NumberAnimation { from: 1; to: 0; duration: 3000; easing.type: Easing.OutQuad }
        }

        MouseArea {
            enabled: disabled
            anchors.fill: parent
            onPressed: stopAnim.restart()
        }
    }

    Image {
        source: "qrc:/img/button_%1_glow.png".arg(iconName)
        anchors.fill: parent
        opacity: selected && !disabled?1.0:0.0

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
    }

    Image {
        source: "qrc:/img/button_%1_bloom.png".arg(iconName)
        anchors.fill: parent
        opacity: 0

        SequentialAnimation on opacity {
            id: bloomAnim
            running: false
            NumberAnimation { from: 0; to: 0.4; duration: 22; easing.type: Easing.InCubic }
            NumberAnimation { from: 0.4; to: 0; duration: 800; easing.type: Easing.OutQuad }
        }

        MouseArea {
            enabled: !disabled
            anchors.fill: parent
            onPressed: {
                bloomAnim.restart()
                bt.clicked()
            }
        }
    }

    Text {
        id: buttonTxt
        horizontalAlignment: Text.AlignHCenter
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(28)
        }
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(15)
        font.family: calaosFont.fontFamilyLight
        font.weight: Font.ExtraLight
        color: "#e7e7e7"
        opacity: selected || disabled?0.4:1
    }

    Image {
        source: "qrc:/img/button_selected_neon.png"
        anchors {
            top: parent.top; topMargin: Units.dp(-5)
            horizontalCenter: parent.horizontalCenter
        }
        opacity: selected?1.0:0.0

        Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
    }
}
