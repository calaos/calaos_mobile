import QtQuick
import SharedComponents

Item {

    property alias labelText: key.text
    property alias valueText: value.text
    property bool small: false

    height: key.height

    anchors {
        left: parent.left; leftMargin: Units.dp(14)
        right: parent.right; rightMargin: Units.dp(14)
    }
    Text {
        id: key
        elide: Text.ElideMiddle
        font.pixelSize: small?Units.dp(12):Units.dp(14)
        font.family: calaosFont.fontFamily
        font.weight: Font.Light
        color: Theme.colorAlpha(Theme.whiteColor, 0.7)
        anchors {
            left: parent.left
            right: value.left; rightMargin: Units.dp(4)
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: value
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        horizontalAlignment: Text.AlignRight
        font.weight: Font.Light
        color: Theme.blueColor
        anchors {
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
    }
}
