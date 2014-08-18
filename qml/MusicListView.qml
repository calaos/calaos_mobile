import QtQuick 2.2
import "calaos.js" as Calaos;


Item {

    property alias model: lst.model
    property alias visibleArea: lst.visibleArea

    ListView {
        id: lst

        width: parent.width
        height: parent.height - header.height
        y: header.height

        spacing: 10 * calaosApp.density

        delegate: Item {

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: -3 * calaosApp.density
            width: parent.width - 16 * calaosApp.density
            height: 200 * calaosApp.density

            BorderImage {
                source: calaosApp.getPictureSized("back_items_home")
                border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
                border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

                anchors.fill: parent
            }

            Image {
                id: icon
                fillMode: Image.PreserveAspectFit
                source: audioCoverSource
                anchors {
                    left: parent.left; leftMargin: 8 * calaosApp.density
                    top: parent.top; topMargin: 8 * calaosApp.density
                    bottom: parent.bottom; bottomMargin: 8 * calaosApp.density
                }
            }

            Text {
                color: "#3ab4d7"
                font { bold: false; pointSize: 13 }
                text: audioName
                clip: true
                elide: Text.ElideRight
                anchors {
                    left: icon.right; leftMargin: 8 * calaosApp.density
                    right: parent.right; rightMargin: 8 * calaosApp.density
                    top: parent.top; topMargin: 18 * calaosApp.density
                }
            }

        }

    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Music")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
