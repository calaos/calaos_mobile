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
