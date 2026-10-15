import QtQuick
import SharedComponents

Item {

    property alias title: titl.text

    height: titl.implicitHeight + Units.dp(10)

    Text {
        id: titl
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: true; pointSize: 12 }
        text: title
        elide: Text.ElideRight
        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            top: parent.top; topMargin: Units.dp(4)
        }
    }
    Rectangle {
        color: "#424242"
        height: Units.dp(2)
        anchors {
            left: parent.left; leftMargin: Units.dp(12)
            right: parent.right; rightMargin: Units.dp(12)
            bottom: parent.bottom
        }
    }
}
