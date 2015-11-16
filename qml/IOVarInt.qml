import QtQuick 2.0

BorderImage {
    property variant modelData

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

    Text {
        id: val
        color: "#ffda5a"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        clip: true
        text: modelData.stateInt + " " + modelData.unit
        elide: Text.ElideNone
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: val.right; leftMargin: 8 * calaosApp.density
            right: modelData.rw?btmin.left:parent.right
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
        imageSource: "button_plus"

        onButtonClicked: modelData.sendInc()

        visible: modelData.rw
    }

    ItemButtonAction {
        id: btmin
        anchors {
            right: btplus.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_min"

        onButtonClicked: modelData.sendDec()

        visible: modelData.rw
    }
}
