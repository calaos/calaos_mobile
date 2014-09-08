import QtQuick 2.0

Item {

    property alias roomItemModel: listView.model

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ItemListView {
        id: listView

        width: parent.width
        height: parent.height
    }

    ScrollBar { listObject: listView }
}
