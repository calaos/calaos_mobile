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

            source: calaosApp.getPictureSized(modelData.stateBool?"icon_bool_on":"icon_bool_off")

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
        }

        SensorStatusIcon {
            sensor: modelData
        }

        ItemButtonAction {
            id: btplus
            imageSource: "button_check"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }

            visible: modelData.rw
        }

        ItemButtonAction {
            id: btmin
            imageSource: "button_empty"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }

            visible: modelData.rw
        }
    }
}
