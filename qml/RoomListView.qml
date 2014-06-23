import QtQuick 2.2
import "calaos.js" as Calaos;

ListView {
    anchors.fill: parent

    spacing: 10 * calaosApp.density

    signal roomClicked(int idx)

    delegate: Item {

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: -3 * calaosApp.density
        width: parent.width - 16 * calaosApp.density
        height: 100 * calaosApp.density

        BorderImage {
            source: "qrc:/img/back_items_home.png"
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            anchors.fill: parent
        }

        BorderImage {
            source: "qrc:/img/back_items_home_glow.png"
            border.left: 15 * calaosApp.density; border.top: 15 * calaosApp.density
            border.right: 15 * calaosApp.density; border.bottom: 15 * calaosApp.density

            anchors.fill: parent
            opacity: 0
            Behavior on opacity { PropertyAnimation { duration: 100 } }

            MouseArea {
                id: ms
                anchors.fill: parent
                hoverEnabled: true
                onPressed: parent.opacity = 1
                onReleased: parent.opacity = 0
                onExited: parent.opacity = 0
                onClicked: roomClicked(index)
            }
        }

        property string roomIconType: roomType
        onRoomIconTypeChanged: roomIcon.source = "qrc:/img/rooms/" + Calaos.getRoomTypeIcon(roomIconType)

        Image {
            id: roomIcon
            fillMode: Image.PreserveAspectFit
            anchors {
                left: parent.left; leftMargin: 8 * calaosApp.density
                top: parent.top; topMargin: 8 * calaosApp.density
                bottom: parent.bottom; bottomMargin: 8 * calaosApp.density
            }
        }

        Text {
            color: "#3ab4d7"
            font { bold: false; pointSize: 13 * calaosApp.density }
            text: roomName
            clip: true
            elide: Text.ElideRight
            anchors {
                left: roomIcon.right; leftMargin: 8 * calaosApp.density
                right: parent.right; rightMargin: 8 * calaosApp.density
                top: parent.top; topMargin: 18 * calaosApp.density
            }
        }

    }

}
