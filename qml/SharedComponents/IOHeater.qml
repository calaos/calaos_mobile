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

            source: calaosApp.getPictureSized(modelData.stateBool?"icon_heater_on":"icon_heater_off")

            Layout.preferredHeight: height
            Layout.preferredWidth: width
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
            iconSource: "qrc:/img/ic_outlet_on.svg"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }
        }

        ItemButtonAction {
            id: btoff
            iconSource: "qrc:/img/ic_outlet_off.svg"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }
        }
    }
}
