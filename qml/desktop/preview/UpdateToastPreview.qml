// Standalone harness to work on UpdateToast.qml without a calaos_server or a
// calaos-container API. It stands in for what main.qml provides at runtime:
// the calaosFont object and a dark background.
//
//   /usr/lib64/qt6/bin/qml -I qml qml/desktop/preview/UpdateToastPreview.qml
//
// (adjust the qml binary path for your distribution, qml6 on some of them)

import QtQuick
import SharedComponents
import ".."

Window {
    id: win

    visible: true
    width: 1024
    height: 768
    color: "#171717"
    title: "UpdateToast preview"

    readonly property string assets: Qt.resolvedUrl("../../../img/")

    //stand-in for the Fonts instance that main.qml exposes as calaosFont
    Item {
        id: calaosFont

        property string fontFamily: fontRegular.name
        property string fontFamilyLight: fontLight.name
        property string fontFamilyMedium: fontMedium.name

        FontLoader { id: fontRegular; source: Qt.resolvedUrl("../../../fonts/Roboto-Regular.ttf") }
        FontLoader { id: fontLight; source: Qt.resolvedUrl("../../../fonts/Roboto-Light.ttf") }
        FontLoader { id: fontMedium; source: Qt.resolvedUrl("../../../fonts/Roboto-Medium.ttf") }
    }

    //approximation of Background.qml, enough to judge contrast
    Rectangle {
        anchors.fill: parent
        color: "#272727"

        Rectangle {
            anchors.fill: parent
            color: "black"
            opacity: 0.4
        }

        BorderImage {
            source: win.assets + "background_center.png"
            opacity: 0.3
            anchors.fill: parent
            border { left: 510; right: 512; top: 2; bottom: 0 }
        }

        Image {
            source: win.assets + "background_quad.png"
            fillMode: Image.Tile
            anchors.fill: parent
        }
    }

    readonly property var scenarios: [
        { label: "3 Calaos + 1 other",
          payload: { count: 4, calaosCount: 3,
                     calaosPackages: ["calaos-home", "calaos-server", "calaos-container"] } },
        { label: "single component",
          payload: { count: 1, calaosCount: 1, calaosPackages: ["calaos-home"] } },
        { label: "system packages only",
          payload: { count: 6, calaosCount: 0, calaosPackages: [] } },
        { label: "long name + overflow",
          payload: { count: 12, calaosCount: 6,
                     calaosPackages: ["calaos-home",
                                      "calaos-server-with-a-very-long-package-name",
                                      "calaos-os"] } }
    ]

    //docked exactly where main.qml puts it
    UpdateToast {
        id: toast

        anchors {
            top: parent.top; topMargin: Units.dp(32)
            right: parent.right; rightMargin: Units.dp(16)
        }

        onAccepted: log.text = "accepted → would push config/update"
        onDismissed: log.text = "dismissed"
    }

    Column {
        spacing: Units.dp(8)

        anchors {
            left: parent.left; leftMargin: Units.dp(30)
            top: parent.top; topMargin: Units.dp(30)
        }

        Repeater {
            model: win.scenarios

            delegate: Rectangle {
                id: scenarioBtn

                required property var modelData

                width: Units.dp(220)
                height: Units.dp(32)
                radius: Units.dp(4)
                color: mouse.containsMouse ? Theme.colorAlpha(Theme.whiteColor, 0.12)
                                           : Theme.colorAlpha(Theme.whiteColor, 0.06)

                Text {
                    anchors { left: parent.left; leftMargin: Units.dp(12); verticalCenter: parent.verticalCenter }
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
                    color: Theme.colorAlpha(Theme.whiteColor, 0.75)
                    text: scenarioBtn.modelData.label
                }

                MouseArea {
                    id: mouse

                    anchors.fill: parent
                    hoverEnabled: true

                    //hide first so the unfold animation replays from scratch
                    onClicked: {
                        toast.hide()
                        replay.payload = scenarioBtn.modelData.payload
                        replay.restart()
                    }
                }
            }
        }

        Text {
            id: log

            topPadding: Units.dp(10)
            font { family: calaosFont.fontFamily; pixelSize: Units.dp(12) }
            color: Theme.colorAlpha(Theme.blueColor, 0.9)
        }
    }

    Timer {
        id: replay

        property var payload

        interval: 500
        onTriggered: {
            log.text = ""
            toast.showToast(replay.payload)
        }
    }

    Component.onCompleted: toast.showToast(win.scenarios[0].payload)
}
