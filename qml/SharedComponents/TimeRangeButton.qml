import QtQuick
import "."

//Flat button of the time range editor: 44 dp targets, no hover needed (kiosk).
Rectangle {
    id: root

    property string fontFamily
    property string text
    //Drawn icon instead of the text: the app fonts lack arrow glyphs
    property string icon
    //Icon drawn before the text, for the named back button and the actions.
    property string leading
    property bool checkable: false
    property bool checked: false
    property bool accent: false
    property color accentColor: Theme.blueColor
    //Text colour of a plain button: blue for the named back button and links.
    property color textColor: Theme.whiteColor
    //Destructive action: red text and border.
    property bool danger: false
    property real sidePadding: Units.dp(24)
    property real textSize: Units.dp(15)

    signal clicked()

    implicitWidth: icon !== "" ? implicitHeight
                 : Math.max(implicitHeight, label.implicitWidth + sidePadding + (leading !== "" ? Units.dp(26) : 0))
    implicitHeight: Units.dp(44)
    radius: Units.dp(4)
    //A disabled primary button keeps its label readable on the accent colour.
    opacity: enabled ? 1 : accent ? 0.6 : 0.4
    color: checked ? Theme.colorAlpha(Theme.blueColor, 0.3)
         : accent ? accentColor : Theme.colorAlpha(Theme.whiteColor, 0.06)
    border.color: activeFocus ? Theme.whiteColor : danger ? Theme.redColor
                 : checked ? Theme.blueColor : Theme.borderColor
    activeFocusOnTab: enabled

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.checkable: checkable
    Accessible.checked: checked
    Accessible.onPressAction: trigger()
    Keys.onReturnPressed: trigger()
    Keys.onEnterPressed: trigger()
    Keys.onSpacePressed: trigger()

    function trigger() {
        if (!enabled)
            return
        //No toggle here: an assignment would replace the parent's binding on
        //`checked`. The parent updates its model and the binding follows.
        clicked()
    }

    ClimateIcon {
        objectName: "leadingIcon"
        visible: root.icon === "" && root.leading !== ""
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: label.left
        anchors.rightMargin: Units.dp(6)
        name: root.leading
        size: Units.dp(20)
        color: label.color
    }

    Text {
        id: label
        visible: root.icon === ""
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.leading !== "" ? Units.dp(13) : 0
        width: Math.min(implicitWidth, root.width - Units.dp(8) - (root.leading !== "" ? Units.dp(26) : 0))
        text: root.text
        color: root.accent && !root.checked ? Theme.backgroundColor : root.danger ? Theme.redColor : root.textColor
        font { family: root.fontFamily; pixelSize: root.textSize; weight: root.accent ? Font.DemiBold : Font.Normal }
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    ClimateIcon {
        objectName: "buttonIcon"
        visible: root.icon !== ""
        anchors.centerIn: parent
        name: root.icon
        size: Units.dp(20)
        color: label.color
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.trigger()
    }
}
