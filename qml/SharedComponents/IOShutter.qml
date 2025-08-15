import QtQuick
import "."

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized(modelData.stateBool?"icon_shutter_on":"icon_shutter_off")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ScrollingText {
        color: !modelData.stateBool?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btup.left; rightMargin: 8 * calaosApp.density
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

        onButtonClicked: {
            feedbackAnim()
            modelData.sendDown()
        }
    }

    ItemButtonAction {
        id: btstop
        anchors {
            right: btdown.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_stop"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendStop()
        }
    }

    ItemButtonAction {
        id: btup
        anchors {
            right: btstop.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_up2"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendUp()
        }
    }
}
