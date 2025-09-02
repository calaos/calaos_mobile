import QtQuick
import QtQuick.Layouts
import "."

ItemBase {
    property variant modelData

    RowLayout {
        id: scenario

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        IconItem {
            id: icon

            source: calaosApp.getPictureSized("icon_scenario")

            Layout.preferredHeight: height
            Layout.preferredWidth: width
        }

        ScrollingText {
            id: name
            color: "#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.ioName
            clip: true
            Layout.fillWidth: true

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
            imageSource: "button_play"

            onButtonClicked: {
                feedbackAnim()
                scenario.flashAnim()
                modelData.sendTrue()
            }
        }
    }
}
