import QtQuick
import SharedComponents

Rectangle {
    property alias glyph: icon.name
    property alias glyphSize: icon.size
    property bool danger: false

    width: Units.dp(40)
    height: width
    radius: Units.dp(10)
    color: Theme.colorAlpha(danger ? Theme.redColor : Theme.blueColor, danger ? 0.14 : 0.18)

    SvgGlyph {
        id: icon
        anchors.centerIn: parent
    }
}
