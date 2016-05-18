import QtQuick 2.0
import "."

ItemBase {
    property variant modelData

    Image {
        id: icon

        source: homeModel.lights_on_count > 0?
                    calaosApp.getPictureSized("icon_light_on"):
                    calaosApp.getPictureSized("icon_light_off")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
        text: lightOnModel.lights_count === 1?
                  qsTr("1 light is on"):
              lightOnModel.lights_count > 1?
                  qsTr("%1 lights are on").arg(lightOnModel.lights_count):
                  qsTr("All lights are off")
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: bt.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bt
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_more"

        onButtonClicked: {
            feedbackAnim()
            openLightsOnView()
        }
    }

}
