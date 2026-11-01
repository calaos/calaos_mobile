import QtQuick
import SharedComponents

//A tile of the settings entry page. Wide, it is a line across the two
//columns. External marks the entries that leave the settings.
CalaosItemBase {
    id: entry

    property string title
    property string subtitle
    property string glyph
    property bool wide: false
    property bool external: false

    readonly property real padH: Units.dp(12)
    readonly property real padV: Units.dp(wide ? 8 : 12)

    Accessible.role: Accessible.Button
    Accessible.name: title

    implicitHeight: wide ?
                        Math.max(Units.dp(60), badge.height + 2 * padV, tileText.implicitHeight + 2 * padV) :
                        Math.max(Units.dp(96), badge.height + Units.dp(8) + tileText.implicitHeight + 2 * padV)

    IconBadge {
        id: badge
        glyph: entry.glyph
        anchors {
            left: parent.left; leftMargin: entry.padH
            top: entry.wide ? undefined : parent.top; topMargin: entry.padV
            verticalCenter: entry.wide ? parent.verticalCenter : undefined
        }
    }

    SvgGlyph {
        objectName: "tileOut"
        visible: entry.external
        name: "out"
        anchors {
            right: parent.right; rightMargin: entry.padH
            top: parent.top; topMargin: entry.padV
        }
    }

    Column {
        id: tileText
        anchors {
            left: entry.wide ? badge.right : parent.left
            leftMargin: entry.wide ? Units.dp(14) : entry.padH
            right: parent.right; rightMargin: entry.padH
            bottom: entry.wide ? undefined : parent.bottom; bottomMargin: entry.padV
            verticalCenter: entry.wide ? parent.verticalCenter : undefined
        }
        spacing: Units.dp(2)

        Text {
            id: tileLabel
            objectName: "tileLabel"
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.whiteColor
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
            text: entry.title
        }

        Text {
            id: tileSubtitle
            objectName: "tileSubtitle"
            width: parent.width
            visible: entry.subtitle !== ""
            wrapMode: Text.WordWrap
            color: Theme.mutedColor
            font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
            text: entry.subtitle
        }
    }
}
