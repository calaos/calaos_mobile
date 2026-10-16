import QtQuick
import SharedComponents

// Non-modal "no network" notice (FIX-10), shown at the top of the content
// while calaosApp.waitingForNetwork: the device has no network and a session
// is wanted, so the app connects by itself once the link is back. It replaces
// the native modal alert, which stayed on screen after the reconnection.
// Slides out of the top edge; the page under it is not pushed down.
Item {
    id: banner

    property bool shown: calaosApp.waitingForNetwork

    // Brief highlight, e.g. when Login is tapped offline: the tap gets an
    // answer without a blocking dialog.
    function highlight() {
        flash.restart()
        Accessible.announce(label.text)
    }

    // A height change is not announced by TalkBack/VoiceOver, unlike the
    // native alert this banner replaces: say it when it appears.
    onShownChanged: if (shown) Accessible.announce(label.text)

    height: shown ? content.height : 0
    clip: true
    visible: height > 0
    Behavior on height { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Rectangle {
        id: content

        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.max(Units.dp(32), label.implicitHeight + Units.dp(12))
        color: Theme.colorAlpha(Theme.backgroundColor, 0.95)

        Rectangle {
            id: flashOverlay
            anchors.fill: parent
            color: Theme.amberColor
            opacity: 0
        }

        Row {
            anchors {
                verticalCenter: parent.verticalCenter
                horizontalCenter: parent.horizontalCenter
            }
            width: Math.min(implicitWidth, parent.width - Units.dp(20))
            spacing: Units.dp(8)

            IconItem {
                id: icon
                width: Units.dp(16); height: Units.dp(16)
                anchors.verticalCenter: parent.verticalCenter
                sourceSize { width: Units.dp(16); height: Units.dp(16) }
                source: calaosApp.getPictureSized("icon_warning") // amber, like the accent
            }

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, content.width - Units.dp(20) - icon.width - parent.spacing)
                text: qsTr("No network, will connect when it is back")
                color: Theme.whiteColor
                font { family: calaosFont.fontFamily; pointSize: 11 }
                elide: Text.ElideRight
            }
        }

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: Units.dp(1)
            color: Theme.amberColor
        }
    }

    SequentialAnimation {
        id: flash
        NumberAnimation { target: flashOverlay; property: "opacity"; to: 0.35; duration: 150; easing.type: Easing.OutQuad }
        NumberAnimation { target: flashOverlay; property: "opacity"; to: 0; duration: 600; easing.type: Easing.InQuad }
    }
}
