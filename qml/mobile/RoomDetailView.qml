import QtQuick
import Calaos
import SharedComponents
import "../background/qml"

Item {

    property alias roomItemModel: listView.model
    property alias filteredRoomItemModel: filterModel.source

    CalaosBackground {
        anchors.fill: parent
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
