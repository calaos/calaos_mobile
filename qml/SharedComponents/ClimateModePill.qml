import QtQuick
import "."

//The current mode, as a button that opens the choice of another. Same frame
//as every Calaos button; greyed when nothing can be chosen.
CalaosItemBase {
    id: root

    property string fontFamily
    property string word
    property string icon: "help"
    property real iconSize: Units.dp(16)
    property real fontSize: Units.dp(14)

    height: Units.dp(36)
    implicitWidth: content.implicitWidth + Units.dp(22)
    text: ""
    opacity: disabled ? 0.45 : 1

    Row {
        id: content
        anchors.centerIn: parent
        spacing: Units.dp(6)

        ClimateIcon {
            objectName: "pillIcon"
            name: root.icon
            size: root.iconSize
            color: Theme.whiteColor
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            objectName: "pillWord"
            color: Theme.blueColor
            font { family: root.fontFamily; weight: Font.Light; pixelSize: root.fontSize }
            text: root.word
            anchors.verticalCenter: parent.verticalCenter
        }

        ClimateIcon {
            name: "chevronD"
            size: root.iconSize
            color: Theme.mutedColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
