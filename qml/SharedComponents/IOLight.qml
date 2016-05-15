import QtQuick 2.0

ItemBase {
    property variant modelData

    AnimatedIcon {
        id: icon

        countImage: 9
        imageFilenameOn: "icon_light_00%1"
        imageFilenameOff: "icon_light_off"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        iconState: modelData.stateBool
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btoff.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
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
        }
        imageSource: "button_light_off"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
        }
    }
}
