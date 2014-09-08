import QtQuick 2.0

BorderImage {
    property variant modelData

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width
    height: 40 * calaosApp.density

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
        font { bold: false; pointSize: 12 }
        text: homeModel.lights_on_count > 0?qsTr("%1 lights are on").arg(homeModel.lights_on_count):qsTr("All lights are off")
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

        onButtonClicked: openLightsOnView()
    }

}
