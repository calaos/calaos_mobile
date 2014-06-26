import QtQuick 2.0

BorderImage {
    property variant modelData

    source: "qrc:/img/back_items_home.png"
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Image {
        id: icon

        source: "qrc:/img/icon_temp.png"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { bold: false; pointSize: 12 * calaosApp.density }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: temp.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: temp
        color: "#ffda5a"
        font { bold: false; pointSize: 12 * calaosApp.density }
        clip: true
        text: modelData.stateInt + " °C"
        elide: Text.ElideNone
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }
}
