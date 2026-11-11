import QtQuick
import SharedComponents

//Version numbers only, so a household session is enough: no admin password
SettingsPage {
    id: settingsPage

    title: qsTr("Versions")
    glyph: "info"

    property bool copied: false

    component VersionLine: Item {
        id: line

        property string name
        property string version

        width: parent ? parent.width : 0
        height: Math.max(Units.dp(48), lineName.implicitHeight + Units.dp(16))

        Rectangle {
            visible: line.Positioner.index > 0
            width: parent.width
            height: 1
            color: Theme.borderColor
        }

        Text {
            id: lineName
            anchors {
                left: parent.left; leftMargin: Units.dp(14)
                right: lineVersion.left; rightMargin: Units.dp(12)
                verticalCenter: parent.verticalCenter
            }
            wrapMode: Text.WrapAnywhere
            color: Theme.whiteColor
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
            text: line.name
        }

        Text {
            id: lineVersion
            objectName: "versionValue"
            anchors {
                right: parent.right; rightMargin: Units.dp(14)
                verticalCenter: parent.verticalCenter
            }
            width: Math.min(implicitWidth, line.width * 0.55)
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignRight
            color: Theme.blueColor
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
            text: line.version
        }
    }

    component Note: Text {
        x: Units.dp(16)
        width: settingsPage.width - Units.dp(32)
        topPadding: Units.dp(8)
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
    }

    Component.onCompleted: versionsModel.refresh()

    Timer {
        id: copiedTimer
        interval: 2500
        onTriggered: settingsPage.copied = false
    }

    Item {
        width: settingsPage.width
        height: Units.dp(60)

        Row {
            anchors {
                left: parent.left; leftMargin: Units.dp(16)
                verticalCenter: parent.verticalCenter
            }
            spacing: Units.dp(12)

            SettingsButton {
                objectName: "copyButton"
                small: true
                text: qsTr("Copy")
                disabled: !versionsModel.loaded
                onButtonClicked: {
                    versionsModel.copyToClipboard()
                    settingsPage.copied = true
                    copiedTimer.restart()
                }
            }

            SettingsButton {
                objectName: "reloadButton"
                small: true
                glyph: "refresh"
                text: qsTr("Reload")
                disabled: versionsModel.loading
                onButtonClicked: versionsModel.refresh()
            }
        }
    }

    Note {
        objectName: "copiedText"
        visible: settingsPage.copied
        topPadding: 0
        color: Theme.blueColor
        text: qsTr("Copied to the clipboard")
    }

    ListGroupHeader { title: qsTr("This phone") }

    SettingsRows {
        VersionLine {
            objectName: "clientLine"
            name: versionsModel.clientName
            version: versionsModel.clientVersion + " (Qt " + versionsModel.qtVersion + ")"
        }
    }

    ListGroupHeader { gap: Units.dp(16); title: qsTr("Box") }

    Note {
        objectName: "stateLoading"
        visible: versionsModel.loading && !versionsModel.loaded
        text: qsTr("Reading the versions of the box…")
    }

    SettingsRows {
        visible: versionsModel.loaded

        VersionLine {
            objectName: "serverLine"
            name: "calaos-server"
            version: versionsModel.serverVersion !== "" ? versionsModel.serverVersion : qsTr("unknown")
        }
        VersionLine {
            visible: versionsModel.systemAvailable && versionsModel.containerVersion !== ""
            name: "calaos-container"
            version: versionsModel.containerVersion
        }
        VersionLine {
            visible: versionsModel.systemAvailable && versionsModel.calaosOs !== ""
            name: "Calaos OS"
            version: versionsModel.calaosOs
        }
        VersionLine {
            visible: versionsModel.systemAvailable && versionsModel.debian !== ""
            name: "Debian"
            version: versionsModel.debian
        }
        VersionLine {
            visible: versionsModel.systemAvailable && versionsModel.kernel !== ""
            name: qsTr("Kernel")
            version: versionsModel.kernel
        }
    }

    Note {
        objectName: "stateMessage"
        visible: versionsModel.loaded && versionsModel.message !== ""
        color: versionsModel.failure === "unreachable" || versionsModel.failure === "unauthorized" ? Theme.redColor : Theme.mutedColor
        text: versionsModel.message
    }

    ListGroupHeader {
        visible: versionsModel.systemAvailable && versionsModel.packages.length > 0
        gap: Units.dp(16)
        title: qsTr("Packages")
    }

    SettingsRows {
        visible: versionsModel.systemAvailable && versionsModel.packages.length > 0

        Repeater {
            objectName: "packageRepeater"
            model: versionsModel.systemAvailable ? versionsModel.packages : []

            VersionLine {
                required property var modelData
                name: modelData.name
                version: modelData.version
            }
        }
    }
}
