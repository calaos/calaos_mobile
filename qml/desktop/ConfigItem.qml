import QtQuick 2.0
import SharedComponents 1.0

Item {
    property alias icon: iconid.source
    property alias title: titleid.text
    property alias subtitle: subtitleid.text

    property bool disabled: false
    property bool active: false

    signal buttonClicked()

    width: bg.width + 20
    height: bg.height + 20

    Image {
        id: bg
        anchors.centerIn: parent
        source: "qrc:/img/config_item.png"

        Image {
            id: iconid

            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.top
                bottomMargin: Units.dp(-72)
            }
        }

        Text {
            id: titleid

            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(14)
            font.family: calaosFont.fontFamily
            horizontalAlignment: Text.AlignHCenter
            font.weight: Font.Light
            color: Theme.blueColor

            anchors {
                left: parent.left; leftMargin: Units.dp(4)
                right: parent.right; rightMargin: Units.dp(4)
                top: parent.top; topMargin: Units.dp(83)
            }
        }

        Text {
            id: subtitleid

            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(12)
            font.family: calaosFont.fontFamily
            horizontalAlignment: Text.AlignHCenter
            font.weight: Font.Light
            color: Theme.colorAlpha(Theme.whiteColor, 0.7)

            anchors {
                left: parent.left; leftMargin: Units.dp(4)
                right: parent.right; rightMargin: Units.dp(4)
                top: parent.top; topMargin: Units.dp(102)
            }
        }

        Image {
            source: "qrc:/img/config_item_select.png"
            opacity: active?1:0
        }

        MouseArea {
            anchors.fill: parent
            onPressed: active = true
            onReleased: active = false
            onClicked: buttonClicked()
            enabled: !disabled
        }
    }

}
