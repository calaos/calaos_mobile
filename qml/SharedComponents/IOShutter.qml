import QtQuick
import QtQuick.Layouts
import "."

ItemBase {
    property variant modelData

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        IconItem {
            id: icon

            source: calaosApp.getPictureSized(modelData.stateBool?"icon_shutter_on":"icon_shutter_off")

            Layout.preferredHeight: height
            Layout.preferredWidth: width
        }

        ScrollingText {
            color: !modelData.stateBool?"#ffda5a":"#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.ioName
            clip: true
            Layout.fillWidth: true

            Behavior on color { ColorAnimation { duration: 200 } }
        }

        SensorStatusIcon {
            sensor: modelData
        }

        ItemButtonAction {
            id: btup
            imageSource: "button_up2"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendUp()
            }
        }

        ItemButtonAction {
            id: btstop
            imageSource: "button_stop"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendStop()
            }
        }

        ItemButtonAction {
            id: btdown
            imageSource: "button_down2"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendDown()
            }
        }
    }
}
