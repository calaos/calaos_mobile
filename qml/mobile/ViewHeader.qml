import QtQuick
import SharedComponents

Rectangle {

    property alias headerLabel: txt.text
    property alias iconSource: ic.source

    anchors { left: parent.left; right: parent.right; top: parent.top }
    color: "#080808"
    height: Units.dp(45)

    Image {
        id: ic

        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left; leftMargin: Units.dp(10)
        }
    }

    Text {
        id: txt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors {
            left: iconSource === ""?parent.left:ic.right
            leftMargin: Units.dp(5)
            verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        color: "#333333"
        height: Units.dp(2)
    }
}
