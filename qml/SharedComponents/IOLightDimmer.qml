import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "."

ItemBase {
    property variant modelData

    height: Units.dp(80)

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(-18)
        }

        AnimatedIcon {
            id: icon

            countImage: 9
            imageFilenameOn: "icon_light_00%1"
            imageFilenameOff: "icon_light_off"

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            iconState: modelData.stateInt > 0
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

        //buttons go from deactivate to activate, left to right: in a RowLayout
        //the declaration order is the visual order, unlike the anchors this
        //code used to rely on
        ItemButtonAction {
            id: btoff
            imageSource: "button_light_off"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }
        }

        ItemButtonAction {
            id: bton
            imageSource: "button_light_on"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }
        }
    }

    CalaosSlider {
        id: slider
        anchors {
            left: parent.left; leftMargin: Units.dp(16)
            right: parent.right; rightMargin: Units.dp(16)
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(18)
            //bottom: parent.bottom; bottomMargin: Units.dp(14)
        }
        live: false
        from: 0; to: 100
        value: modelData.stateInt
        onValueChanged: {
            if (value !== modelData.stateInt) {
                feedbackAnim()
                modelData.sendIntValue(Math.round(value))
            }
        }
    }
}
