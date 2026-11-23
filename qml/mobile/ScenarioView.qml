import QtQuick
import SharedComponents
import "../background/qml"

Item {
    CalaosBackground {
        anchors.fill: parent
    }

    ItemListView {
        id: listViewItems
        model: scenarioModel

        y: header.height
        width: parent.width
        height: parent.height - header.height
    }

    ScrollBar { listObject: listViewItems }

    ViewHeader {
        id: header
        headerLabel: qsTr("Scenarios")
        iconSource: calaosApp.getPictureSized("icon_scenario")
    }
}
