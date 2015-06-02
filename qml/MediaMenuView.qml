import QtQuick 2.0

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
        spacing: 10 * calaosApp.density

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

        width: Math.floor(parent.width / (198 * calaosApp.density)) * 198 * calaosApp.density
        height: parent.height - header.height
        y: header.height

        anchors.centerIn: parent
        anchors.verticalCenterOffset: 30 * calaosApp.density
    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Media")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
