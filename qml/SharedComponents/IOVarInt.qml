import QtQuick
import "."
import QuickFlux
import "../quickflux"

ItemBase {
    property variant modelData

    Text {
        id: val
        color: "#ffda5a"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        clip: true
        text: modelData.stateInt + " " + modelData.unit
        elide: Text.ElideNone
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ScrollingText {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        anchors {
            left: val.right; leftMargin: 8 * calaosApp.density
            right: iconWarning.right
            rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    IconItem {
        id: iconWarning

        source: calaosApp.getPictureSized("icon_warning")

        anchors {
            rightMargin: 8 * calaosApp.density
            right: modelData.rw?btmin.left:parent.right
            verticalCenter: parent.verticalCenter
        }

        visible: modelData.hasWarning

        MouseArea {
            anchors.fill: parent
            onClicked: AppActions.showNotificationMsg(qsTr("Warning"), qsTr("The value has not been updated for a long time"), qsTr("Close"))
        }
    }

    ItemButtonAction {
        id: btplus
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_plus"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendInc()
        }

        visible: modelData.rw
    }

    ItemButtonAction {
        id: btmin
        anchors {
            right: btplus.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_min"

        onButtonClicked:  {
            feedbackAnim()
            modelData.sendDec()
        }

        visible: modelData.rw
    }
}
