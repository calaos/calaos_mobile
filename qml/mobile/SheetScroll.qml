import QtQuick

//The body of a sheet: as tall as the sheet, or as its content when that does not
//fit (a phone held sideways), in which case it scrolls.
Flickable {
    id: scroll

    //Height the content wants; the content item gets at least the height of the sheet
    property real implicitContentHeight: 0
    default property alias content: holder.data

    clip: true
    contentWidth: width
    contentHeight: Math.max(height, implicitContentHeight)
    interactive: contentHeight > height
    boundsBehavior: Flickable.StopAtBounds

    Item {
        id: holder
        width: scroll.width
        height: scroll.contentHeight
    }
}
