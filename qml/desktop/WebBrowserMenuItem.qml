import QtQuick
import SharedComponents

Item {
    id: menuItem

    property string text: ""
    property string icon: ""

    signal clicked()

    width: parent.width
    height: Units.dp(40)

    Rectangle {
        anchors.fill: parent
        anchors.margins: Units.dp(4)
        color: mouseArea.pressed ? "#3AB4D7" : "transparent"
        opacity: mouseArea.pressed ? 0.15 : 0.0
        radius: Units.dp(2)

        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }
    }

    Row {
        anchors {
            left: parent.left
            leftMargin: Units.dp(12)
            verticalCenter: parent.verticalCenter
        }
        spacing: Units.dp(10)

        Image {
            source: menuItem.icon
            width: Units.dp(18)
            height: Units.dp(18)
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: menuItem.text
            font.pixelSize: Units.dp(14)
            font.family: calaosFont.fontFamily
            font.weight: Font.ExtraLight
            color: mouseArea.pressed ? "#3AB4D7" : "#e7e7e7"
            anchors.verticalCenter: parent.verticalCenter

            Behavior on color {
                ColorAnimation { duration: 100 }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: menuItem.clicked()
    }
}
