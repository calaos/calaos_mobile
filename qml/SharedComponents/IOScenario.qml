import QtQuick 2.0
import "."

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized("icon_scenario")

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
            right: btplay.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 100 } }
    }

    function flashAnim() {
        name.color = "#ffda5a"
        timer.start()
    }

    Timer {
        id: timer
        running: false
        interval: 1500
        onTriggered: name.color = "#3ab4d7"
        repeat: false
    }

    ItemButtonAction {
        id: btplay
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_play"

        onButtonClicked: {
            feedbackAnim()
            flashAnim()
            modelData.sendTrue()
        }
    }
}
