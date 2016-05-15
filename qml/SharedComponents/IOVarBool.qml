import QtQuick 2.0

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized(modelData.stateBool?"icon_bool_on":"icon_bool_off")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btmin.left
            rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ItemButtonAction {
        id: btplus
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_check"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendTrue()
        }

        visible: modelData.rw
    }

    ItemButtonAction {
        id: btmin
        anchors {
            right: btplus.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_empty"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
        }

        visible: modelData.rw
    }
}
