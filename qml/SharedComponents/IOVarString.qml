import QtQuick
import Calaos
import "."

ItemBase {
    property variant modelData

    ScrollingText {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.stateString == ""?modelData.ioName:modelData.stateString
        clip: true
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            right: modelData.rw?btkb.left:parent.right
            rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ItemButtonAction {
        id: btkb
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_keyboard"

        onButtonClicked: {
            feedbackAnim()
            modelData.askStateText()
        }

        visible: (modelData.rw || modelData.ioType === Common.StringOut) &&
                 modelData.ioType !== Common.StringIn
    }
}
