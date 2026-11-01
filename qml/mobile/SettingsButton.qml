import QtQuick
import SharedComponents

CalaosItemBase {
    id: button

    property string glyph
    property bool small: false

    Accessible.role: Accessible.Button
    Accessible.name: text

    implicitWidth: Math.max(Units.dp(90), buttonContent.implicitWidth + Units.dp(28))
    implicitHeight: Units.dp(small ? 44 : 52)
    height: implicitHeight

    __rectBorder.visible: false
    __label.visible: false

    Rectangle {
        anchors.fill: parent
        radius: Units.dp(8)
        color: "transparent"
        border {
            width: Units.dp(2)
            color: button.disabled ? Theme.borderColor : Theme.colorAlpha(Theme.blueColor, 0.4)
        }
    }

    Row {
        id: buttonContent
        anchors.centerIn: parent
        spacing: Units.dp(10)

        SvgGlyph {
            anchors.verticalCenter: parent.verticalCenter
            visible: button.glyph !== ""
            size: Units.dp(20)
            name: button.glyph
        }

        Text {
            objectName: "buttonLabel"
            anchors.verticalCenter: parent.verticalCenter
            color: button.disabled ? Theme.mutedColor : Theme.blueColor
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
            text: button.text
        }
    }
}
