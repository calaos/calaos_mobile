import QtQuick 2.0

BorderImage {
    property variant modelData

    source: "qrc:/img/back_items_home.png"
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Text {
        color: "#3ab4d7"
        font { bold: false; pointSize: 12 }
        text: "LIGHT: " + modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: parent.left; leftMargin: 8
            right: parent.right; rightMargin: 8
            verticalCenter: parent.verticalCenter
        }
    }
}
