import QtQuick
import Calaos
import SharedComponents

Item {

    property alias roomItemModel: listView.model
    property alias filteredRoomItemModel: filterModel.source

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

        model: RoomFilterModel {
            id: filterModel
            filter: Common.FilterAll
            scenarioVisible: true
        }
    }

    ScrollBar { listObject: listView }
}
