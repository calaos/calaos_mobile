import QtQuick 2.0

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized("icon_temp")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
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
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        clip: true
        text: modelData.stateInt + " °C"
        elide: Text.ElideNone
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }
}
