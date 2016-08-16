import QtQuick 2.0
import QtQuick.Controls 1.2
import "."

ItemBase {
    property variant modelData

    height: 80 * calaosApp.density

    AnimatedIcon {
        id: icon

        countImage: 9
        imageFilenameOn: "icon_light_00%1"
        imageFilenameOff: "icon_light_off"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -18 * calaosApp.density
        }

        iconState: modelData.stateInt > 0
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btoff.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -18 * calaosApp.density
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -18 * calaosApp.density
        }
        imageSource: "button_light_on"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendTrue()
        }
    }

    ItemButtonAction {
        id: btoff
        anchors {
            right: bton.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -18 * calaosApp.density
        }
        imageSource: "button_light_off"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
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
        updateValueWhileDragging: false
        maximumValue: 100
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
