import QtQuick 2.2
import Calaos 1.0
import QtQuick.Controls 1.2

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
