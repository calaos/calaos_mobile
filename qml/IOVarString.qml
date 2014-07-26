import QtQuick 2.0

BorderImage {
    property variant modelData

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Text {
        id: name
        color: "#3ab4d7"
        font { bold: false; pointSize: 12 * calaosApp.density }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            right: modelData.rw?btkb.left:parent.right
            rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ItemButtonAction {
        id: btkb
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_keyboard"

        onButtonClicked: console.log("TODO")

        visible: modelData.rw
    }
}
