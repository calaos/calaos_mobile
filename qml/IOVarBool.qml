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

        source: modelData.stateBool?"qrc:/img/icon_bool_on.png":"qrc:/img/icon_bool_off.png"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        fillMode: Image.PreserveAspectFit
        height: sourceSize.height * calaosApp.density
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
            right: btmin.left
            rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ItemButtonAction {
        id: btplus
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_check.png"

        onButtonClicked: modelData.sendTrue()

        visible: modelData.rw
    }

    ItemButtonAction {
        id: btmin
        anchors {
            right: btplus.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_empty.png"

        onButtonClicked: modelData.sendFalse()

        visible: modelData.rw
    }
}
