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
        height: 40 * calaosApp.density

        Text {
            color: "#3ab4d7"
            font { bold: false; pointSize: 12 }
            text: ioName
            clip: true
            elide: Text.ElideMiddle
            anchors {
                left: parent.left; leftMargin: 8
                right: parent.right; rightMargin: 8
                verticalCenter: parent.verticalCenter
            }
        }
    }

}
