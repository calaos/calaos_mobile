import QtQuick
import SharedComponents

//What the settings pages share: background, header and a scrolling column
//that receives the children.
Item {
    id: settingsPage

    property alias title: header.headerLabel
    property alias glyph: header.glyph
    default property alias content: column.data

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: listFlick

        Column {
            id: column
            width: parent.width
            topPadding: Units.dp(8)
            bottomPadding: Units.dp(16)
        }

        contentHeight: column.implicitHeight
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height
        flickableDirection: Flickable.VerticalFlick
    }

    KeyboardScroller { flickable: listFlick }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
    }
}
