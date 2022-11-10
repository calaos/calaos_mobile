import QtQuick
import "."
import QuickFlux
import "../quickflux"

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
            right: iconWarning.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    IconItem {
        id: iconWarning

        source: calaosApp.getPictureSized("icon_warning")

        anchors {
            right: temp.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        visible: modelData.hasWarning

        MouseArea {
            anchors.fill: parent
            onClicked: AppActions.showNotificationMsg(qsTr("Warning"), qsTr("The value has not been updated for a long time"), qsTr("Close"))
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
