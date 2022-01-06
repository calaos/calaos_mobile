import QtQuick
import Calaos
import QtQuick.Controls
import SharedComponents

Item {

    property variant modelData

    Rectangle {
        color: "black"
        anchors.fill: parent
    }

    Item {
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
