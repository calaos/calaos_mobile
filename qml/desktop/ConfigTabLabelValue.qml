import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1

RowLayout {

    property alias labelText: key.text
    property alias valueText: value.text

    anchors {
        left: parent.left; leftMargin: Units.dp(14)
        right: parent.right; rightMargin: Units.dp(14)
    }
    Text {
        id: key
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        font.weight: Font.Light
        color: Theme.colorAlpha(Theme.whiteColor, 0.7)
    }

    Item { Layout.fillWidth: true; height: 1 }

    Text {
        id: value
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        font.weight: Font.Light
        color: Theme.blueColor
    }
}
