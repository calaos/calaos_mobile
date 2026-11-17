import QtQuick
import "."

//Flat button of the time range editor: 44 dp targets, no hover needed (kiosk).
Rectangle {
    id: root

    property string fontFamily
    property string text
    property bool checkable: false
    property bool checked: false
    property bool accent: false
    //Destructive action: red text and border.
    property bool danger: false

    signal clicked()

    implicitWidth: Math.max(Units.dp(44), label.implicitWidth + Units.dp(24))
    implicitHeight: Units.dp(44)
    radius: Units.dp(4)
    opacity: enabled ? 1 : 0.4
    color: checked ? Theme.colorAlpha(Theme.blueColor, 0.3)
         : accent ? Theme.blueColor : Theme.colorAlpha(Theme.whiteColor, 0.06)
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

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.min(implicitWidth, root.width - Units.dp(8))
        text: root.text
        color: root.accent && !root.checked ? "#111111" : root.danger ? Theme.redColor : Theme.whiteColor
        font { family: root.fontFamily; pixelSize: Units.dp(15) }
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.trigger()
    }
}
