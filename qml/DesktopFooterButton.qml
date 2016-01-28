import QtQuick 2.3
import Units 1.0

Image {

    property alias label: txt.text
    property alias icon: ic.source

    signal btClicked()

    height: Units.dp(34)
    width: txt.implicitWidth + Units.dp(50) + ic.implicitWidth
    //width: 250

    source: "qrc:/img/button_action_pattern.png"

    fillMode: Image.Tile
    horizontalAlignment: Image.AlignLeft
    verticalAlignment: Image.AlignTop

    BorderImage {
        source: "qrc:/img/button_action_border.png"

        anchors.fill: parent

        border.left: Units.dp(5); border.top: Units.dp(16)
        border.right: Units.dp(5); border.bottom: Units.dp(16)
    }

    Image {
        id: ic

        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left; leftMargin: Units.dp(5)
        }
    }

    Text {
        id: txt
        color: "#e7e7e7"
        horizontalAlignment: Text.AlignLeft
        font { family: calaosFont.fontFamilyLight; bold: true; pointSize: 12 }

        anchors {
            left: ic.right; rightMargin: Units.dp(5)
            verticalCenter: parent.verticalCenter
        }
    }

    BorderImage {
        id: neon
        source: "qrc:/img/button_action_neon.png"

        anchors.fill: parent
        opacity: 0
        border.left: Units.dp(5); border.top: Units.dp(16)
        border.right: Units.dp(5); border.bottom: Units.dp(16)
    }

    ParallelAnimation {
        id: pressAnim
        running: false
        ColorAnimation {
            /*from: "#e7e7e7";*/ to: "#3AB4D7"; duration: 22; easing.type: Easing.InCubic
            target: txt; property: "color"
        }
        NumberAnimation {
            /*from: 0;*/ to: 1.0; duration: 22; easing.type: Easing.InCubic
            target: neon; property: "opacity"
        }
    }

    ParallelAnimation {
        id: releaseAnim
        running: false
        ColorAnimation {
            to: "#e7e7e7"; duration: 800; easing.type: Easing.OutQuad
            target: txt; property: "color"
        }
        NumberAnimation {
            to: 0; duration: 800; easing.type: Easing.OutQuad
            target: neon; property: "opacity"
        }
    }

    MouseArea {
        id: ms
        anchors.fill: parent
        onPressed: {
            releaseAnim.stop()
            pressAnim.start()
        }
        onReleased: {
            pressAnim.stop()
            releaseAnim.start()
        }
        onClicked: btClicked()
    }
}
