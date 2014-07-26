import QtQuick 2.0

BorderImage {
    property variant modelData

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Image {
        id: icon

        source: calaosApp.getPictureSized(modelData.stateBool?"icon_shutter_on":"icon_shutter_off")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
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
        imageSource: "button_down2"

        onButtonClicked: modelData.sendDown()
    }

    ItemButtonAction {
        id: btstop
        anchors {
            right: btdown.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_stop"

        onButtonClicked: modelData.sendStop()
    }

    ItemButtonAction {
        id: btup
        anchors {
            right: btstop.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_up2"

        onButtonClicked: modelData.sendUp()
    }
}
