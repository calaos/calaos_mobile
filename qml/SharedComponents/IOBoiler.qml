import QtQuick 2.5
import "."

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized(modelData.stateBool?"icon_boiler_on":"icon_boiler_off")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
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
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        iconSource: "qrc:/img/ic_outlet_on.svg"

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
        iconSource: "qrc:/img/ic_outlet_off.svg"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
        }
    }
}
