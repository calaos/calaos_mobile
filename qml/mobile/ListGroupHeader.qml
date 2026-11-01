import QtQuick
import SharedComponents

Item {
    property alias title: titl.text
    property bool danger: false
    //Space above, when the group follows another one
    property real gap: 0

    width: parent ? parent.width : 0
    height: gap + Units.dp(12) + titl.implicitHeight + Units.dp(6) + rule.height + Units.dp(8)

    Text {
        id: titl
        color: danger ? Theme.redColor : Theme.blueColor
        font { family: calaosFont.fontFamily; bold: true; pointSize: 12 }
        wrapMode: Text.WordWrap
        anchors {
            left: parent.left; leftMargin: Units.dp(16)
            right: parent.right; rightMargin: Units.dp(16)
            top: parent.top; topMargin: gap + Units.dp(12)
        }
    }
    Rectangle {
        id: rule
        color: danger ? Theme.colorAlpha(Theme.redColor, 0.45) : Theme.borderColor
        height: Units.dp(2)
        anchors {
            left: parent.left
            right: parent.right
            top: titl.bottom; topMargin: Units.dp(6)
        }
    }
}
