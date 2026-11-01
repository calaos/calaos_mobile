import QtQuick
import SharedComponents

CalaosItemBase {
    id: row

    property string title
    property string glyph
    property bool chevron: false
    property bool danger: false

    Accessible.role: Accessible.Button
    Accessible.name: title

    width: parent ? parent.width : 0
    height: Math.max(Units.dp(64), rowLabel.implicitHeight + Units.dp(16))
    __rectBorder.visible: false

    Rectangle {
        visible: row.Positioner.index > 0
        width: parent.width
        height: 1
        color: Theme.borderColor
    }

    IconBadge {
        id: rowIcon
        glyph: row.glyph
        danger: row.danger
        anchors {
            left: parent.left; leftMargin: Units.dp(12)
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: rowLabel
        anchors {
            left: rowIcon.right; leftMargin: Units.dp(14)
            right: rowChevron.visible ? rowChevron.left : parent.right
            rightMargin: rowChevron.visible ? Units.dp(14) : Units.dp(12)
            verticalCenter: parent.verticalCenter
        }
        wrapMode: Text.WordWrap
        color: row.danger ? Theme.redColor : Theme.whiteColor
        font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
        text: row.title
    }

    SvgGlyph {
        id: rowChevron
        visible: row.chevron
        name: "chev"
        anchors {
            right: parent.right; rightMargin: Units.dp(12)
            verticalCenter: parent.verticalCenter
        }
    }
}
