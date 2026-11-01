import QtQuick
import Calaos
import QtQuick.Controls
import SharedComponents

Item {
    id: page

    property variant modelData

    readonly property bool onScreen: StackView.status === StackView.Active ||
                                     StackView.status === StackView.Activating
    readonly property real pixelRatio: Screen.devicePixelRatio

    CameraFeed {
        camera: page.modelData ? page.modelData : null
        active: page.onScreen
        frameSize: Qt.size(pictureArea.width * page.pixelRatio,
                           pictureArea.height * page.pixelRatio)
    }

    Rectangle {
        color: "black"
        anchors.fill: parent
    }

    Item {
        id: pictureArea

        width: parent.width
        height: parent.height - header.height
        y: header.height

        Image {
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit

            source: modelData.url_single
        }
    }

    ViewHeader {
        id: header
        headerLabel: modelData.name
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
