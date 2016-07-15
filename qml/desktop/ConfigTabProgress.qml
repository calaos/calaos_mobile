import QtQuick 2.5
import SharedComponents 1.0

Item {

    property alias labelText: key.text
    property alias valueText: value.text
    property alias secondValueText: secondValue.text
    property int progress: 50 //0-100

    height: key.height + secondValue.height + Units.dp(2)

    anchors {
        left: parent.left; leftMargin: Units.dp(14)
        right: parent.right; rightMargin: Units.dp(14)
    }
    Text {
        id: key
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(14)
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

    Text {
        id: secondValue
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(12)
        font.family: calaosFont.fontFamily
        horizontalAlignment: Text.AlignRight
        font.weight: Font.Light
        color: Theme.whiteColor
        opacity: 0.4
        anchors {
            right: parent.right
            top: value.bottom; topMargin: Units.dp(2)
        }
    }

    Rectangle {
        id: bgprogress
        color: "transparent"
        border.color: Theme.blueColor
        border.width: Units.dp(1)
        opacity: 0.3
        height: Units.dp(4)
        width: parent.width * 0.7

        anchors {
            left: parent.left
            verticalCenter: secondValue.verticalCenter
        }
    }

    Item {
        id: prog
        anchors {
            left: bgprogress.left; leftMargin: bgprogress.border.width
            top: bgprogress.top; topMargin: bgprogress.border.width
            bottom: bgprogress.bottom; bottomMargin: bgprogress.border.width
            right: bgprogress.right; rightMargin: bgprogress.border.width
        }

        Rectangle {
            anchors {
                top: parent.top; bottom: parent.bottom
                left: parent.left
            }

            color: Theme.blueColor
            width: (prog.width * progress) / 100
        }
    }
}
