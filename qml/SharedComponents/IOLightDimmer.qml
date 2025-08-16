import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "."

ItemBase {
    property variant modelData

    height: 80 * calaosApp.density

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

    CalaosSlider {
        id: slider
        anchors {
            left: parent.left; leftMargin: 16 * calaosApp.density
            right: parent.right; rightMargin: 16 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: 18 * calaosApp.density
            //bottom: parent.bottom; bottomMargin: 14 * calaosApp.density
        }
        live: false
        from: 0; to: 100
        value: modelData.stateInt
        onValueChanged: {
            console.log("slider value changed")
            if (value !== modelData.stateInt) {
                feedbackAnim()
                modelData.sendIntValue(Math.round(value))
            }
        }
    }
}
