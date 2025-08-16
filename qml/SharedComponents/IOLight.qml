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

        AnimatedIcon {
            id: icon

            countImage: 9
            imageFilenameOn: "icon_light_00%1"
            imageFilenameOff: "icon_light_off"

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            iconState: modelData.stateBool
        }

        ScrollingText {
            color: icon.iconState?"#ffda5a":"#3ab4d7"
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
            id: bton
            imageSource: "button_light_on"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }
        }

        ItemButtonAction {
            id: btoff
            imageSource: "button_light_off"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }
        }
    }
}
