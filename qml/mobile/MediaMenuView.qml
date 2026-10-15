import QtQuick
import SharedComponents

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flow {
        id: listViewItems
        spacing: Units.dp(10)

        MediaMenuItem {
            label: qsTr("Music")
            icon: IconMusic {}
            onButtonClicked: stackView.push(musicView)
        }

        MediaMenuItem {
            label: qsTr("Surveillance")
            icon: IconCamera {}
            onButtonClicked: {
                cameraModel.cameraVisible = true
                stackView.push(cameraView)
            }
        }

        width: Math.floor(parent.width / (Units.dp(198))) * Units.dp(198)
        height: parent.height - header.height
        y: header.height

        anchors.centerIn: parent
        anchors.verticalCenterOffset: Units.dp(30)
    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Media")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
