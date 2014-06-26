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

        source: modelData.stateBool?"qrc:/img/icon_shutter_on.png":"qrc:/img/icon_shutter_off.png"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        fillMode: Image.PreserveAspectFit
        height: sourceSize.height * calaosApp.density
    }

    Text {
        color: !modelData.stateBool?"#ffda5a":"#3ab4d7"
        font { bold: false; pointSize: 12 * calaosApp.density }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btdown.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: btdown
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_down2.png"

        onButtonClicked: modelData.sendDown()
    }

    ItemButtonAction {
        id: btstop
        anchors {
            right: btdown.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_stop.png"

        onButtonClicked: modelData.sendStop()
    }

    ItemButtonAction {
        id: btup
        anchors {
            right: btstop.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "qrc:/img/button_up2.png"

        onButtonClicked: modelData.sendUp()
    }
}
