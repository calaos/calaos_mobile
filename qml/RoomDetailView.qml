import QtQuick 2.0

Item {

    property alias headerLabel: header.headerLabel

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ItemListView {
        id: listView

        model: roomModel

        y: header.height
        width: parent.width
        height: parent.height - header.height
    }

    ScrollBar { listObject: listView }

    ViewHeader {
        id: header
        iconSource: calaosApp.getPictureSized("icon_room")
    }
}
