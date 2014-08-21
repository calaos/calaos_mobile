import QtQuick 2.0
import QtQuick.Controls 1.2

BorderImage {
    property variant modelData

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 120 * calaosApp.density

    AnimatedIcon {
        id: icon

        countImage: 9
        imageFilenameOn: "icon_light_00%1"
        imageFilenameOff: "icon_light_off"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40
        }

        iconState: modelData.stateInt > 0
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { bold: false; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btoff.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40
        }
        imageSource: "button_light_on"

        onButtonClicked: modelData.sendTrue()
    }

    ItemButtonAction {
        id: btoff
        anchors {
            right: bton.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: -40
        }
        imageSource: "button_light_off"

        onButtonClicked: modelData.sendFalse()
    }

    Rectangle {
        color: "#ff3333"
        height: slider_red.height
        width: height
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: slider_red.verticalCenter
        }
    }

    Slider {
        id: slider_red
        anchors {
            left: parent.left; leftMargin: 28 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            bottom: slider_green.top; bottomMargin: 8 * calaosApp.density
        }
        updateValueWhileDragging: false
        maximumValue: 100
        value: modelData.stateRed
        onValueChanged: {
            console.log("red slider value changed")
            if (value !== modelData.stateRed)
                modelData.sendValueRed(Math.round(value))
        }
    }

    Rectangle {
        color: "#32e677"
        height: slider_green.height
        width: height
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: slider_green.verticalCenter
        }
    }
    Slider {
        id: slider_green
        anchors {
            left: parent.left; leftMargin: 28 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            bottom: slider_blue.top; bottomMargin: 8 * calaosApp.density
        }
        updateValueWhileDragging: false
        maximumValue: 100
        value: modelData.stateGreen
        onValueChanged: {
            console.log("green slider value changed")
            if (value !== modelData.stateGreen)
                modelData.sendValueGreen(Math.round(value))
        }
    }

    Rectangle {
        color: "#339dff"
        height: slider_blue.height
        width: height
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: slider_blue.verticalCenter
        }
    }
    Slider {
        id: slider_blue
        anchors {
            left: parent.left; leftMargin: 28 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            bottom: parent.bottom; bottomMargin: 14 * calaosApp.density
        }
        updateValueWhileDragging: false
        maximumValue: 100
        value: modelData.stateBlue
        onValueChanged: {
            console.log("blue slider value changed")
            if (value !== modelData.stateBlue)
                modelData.sendValueBlue(Math.round(value))
        }
    }
}
