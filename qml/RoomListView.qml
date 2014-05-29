import QtQuick 2.2
import "calaos.js" as Calaos;

ListView {
    anchors.fill: parent

    spacing: 10 * calaosApp.density

    delegate: BorderImage {
        source: "qrc:/img/back_items_home.png"
        border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
        border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 10 * calaosApp.density
        height: 100 * calaosApp.density

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
