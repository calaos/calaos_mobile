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

        Image {
            id: icon

            source: homeModel.lights_on_count > 0?
                        calaosApp.getPictureSized("icon_light_on"):
                        calaosApp.getPictureSized("icon_light_off")

            Layout.preferredHeight: height
            Layout.preferredWidth: width
        }

        ScrollingText {
            color: icon.iconState?"#ffda5a":"#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: lightOnModel.lights_count === 1?
                      qsTr("1 light is on"):
                  lightOnModel.lights_count > 1?
                      qsTr("%1 lights are on").arg(lightOnModel.lights_count):
                      qsTr("All lights are off")
            clip: true
            Layout.fillWidth: true

            Behavior on color { ColorAnimation { duration: 200 } }
        }

        ItemButtonAction {
            id: bt
            imageSource: "button_more"

            onButtonClicked: {
                feedbackAnim()
                openLightsOnView()
            }
        }
    }
}
