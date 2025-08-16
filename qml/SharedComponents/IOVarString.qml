import QtQuick
import Calaos
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

        ScrollingText {
            id: name
            color: "#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.stateString == ""?modelData.ioName:modelData.stateString
            clip: true
            Layout.fillWidth: true
        }

        SensorStatusIcon {
            sensor: modelData
        }

        ItemButtonAction {
            id: btkb
            imageSource: "button_keyboard"

            onButtonClicked: {
                feedbackAnim()
                modelData.askStateText()
            }

            visible: (modelData.rw || modelData.ioType === Common.StringOut) &&
                     modelData.ioType !== Common.StringIn
        }
    }
}
