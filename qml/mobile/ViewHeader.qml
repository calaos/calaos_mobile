import QtQuick
import SharedComponents

Rectangle {

    property alias headerLabel: txt.text
    property alias iconSource: ic.source
    //An icon of img/settings, in place of iconSource
    property alias glyph: glyphIcon.name

    anchors { left: parent.left; right: parent.right; top: parent.top }
    color: "#080808"
    height: Units.dp(45)

    Image {
        id: ic

        visible: glyph === ""
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left; leftMargin: Units.dp(10)
        }
    }

    SvgGlyph {
        id: glyphIcon

        visible: name !== ""
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left; leftMargin: Units.dp(16)
        }
    }

    Text {
        id: txt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors {
            left: glyph !== "" ? glyphIcon.right : (iconSource === "" ? parent.left : ic.right)
            leftMargin: glyph !== "" ? Units.dp(12) : Units.dp(5)
            verticalCenter: parent.verticalCenter
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        color: "#333333"
        height: Units.dp(2)
    }
}
