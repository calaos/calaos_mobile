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
        id: listViewFav
        model: favoritesModel

        y: header.height
        width: parent.width
        height: parent.height - header.height
    }

    Text {
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors.centerIn: listViewFav
        text: qsTr("No favorites yet.<br>Go to <b>Config</b> to add some.")
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        visible: listViewFav.count == 0
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
        width: parent.width
    }

    ScrollBar { listObject: listViewFav }

    ViewHeader {
        id: header
        headerLabel: qsTr("Favorites")
        iconSource: calaosApp.getPictureSized("fav")
    }
}
