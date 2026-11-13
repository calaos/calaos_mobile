import QtQuick
import SharedComponents
import QtQuick.Layouts

PageScaffold {
    id: page

    title: qsTr("Versions")

    // The dpkg package list is the only place Calaos components appear; the
    // server and container lines are a fallback for when it is absent.
    readonly property bool packagesListed: versionsModel.systemAvailable && versionsModel.packages.length > 0

    component BodyText: Text {
        font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
        color: Theme.colorAlpha(Theme.whiteColor, 0.85)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component VersionRow: RowLayout {
        property alias name: rowName.text
        property alias version: rowVersion.text

        spacing: Units.dp(12)
        Layout.fillWidth: true

        Text {
            id: rowName
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
            color: Theme.colorAlpha(Theme.whiteColor, 0.7)
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Text {
            id: rowVersion
            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
            color: Theme.blueColor
            horizontalAlignment: Text.AlignRight
        }
    }

    Component.onCompleted: versionsModel.refresh()

    footerButtons: [
        FooterButton {
            objectName: "refresh"
            label: qsTr("Reload")
            icon: "qrc:/img/button_action_reload.png"
            Layout.minimumWidth: width
            onBtClicked: versionsModel.refresh()
        },
        FooterButton {
            label: qsTr("Back to config")
            icon: "qrc:/img/button_action_back.png"
            Layout.minimumWidth: width
            onBtClicked: rootWindow.handleBack()
        },
        FooterButton {
            label: qsTr("Quit")
            icon: "qrc:/img/button_action_quit.png"
            Layout.minimumWidth: width
            onBtClicked: rootWindow.goToDesktop()
        }
    ]

    BorderImage {
        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: tabs.left; rightMargin: Units.dp(20)
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                leftMargin: Units.dp(30); rightMargin: Units.dp(30)
                topMargin: Units.dp(14); bottomMargin: Units.dp(14)
            }
            clip: true

            Flickable {
                id: pane

                anchors.fill: parent
                contentHeight: column.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: column

                    width: pane.width - Units.dp(12)
                    spacing: Units.dp(10)

                    VersionRow {
                        objectName: "clientRow"
                        // calaos-home is in the package list when it is shown
                        visible: !page.packagesListed
                        name: qsTr("This screen")
                        version: versionsModel.clientVersion + " (Qt " + versionsModel.qtVersion + ")"
                    }

                    BodyText {
                        objectName: "stateLoading"
                        text: qsTr("Reading the versions of the box…")
                        visible: versionsModel.loading && !versionsModel.loaded
                    }

                    VersionRow {
                        objectName: "serverRow"
                        visible: versionsModel.loaded && !page.packagesListed
                        name: "calaos-server"
                        version: versionsModel.serverVersion !== "" ? versionsModel.serverVersion : qsTr("unknown")
                    }

                    VersionRow {
                        objectName: "containerRow"
                        visible: versionsModel.systemAvailable && !page.packagesListed && versionsModel.containerVersion !== ""
                        name: "calaos-container"
                        version: versionsModel.containerVersion
                    }

                    VersionRow {
                        visible: versionsModel.systemAvailable && versionsModel.calaosOs !== ""
                        name: "Calaos OS"
                        version: versionsModel.calaosOs
                    }

                    VersionRow {
                        visible: versionsModel.systemAvailable && versionsModel.debian !== ""
                        name: "Debian"
                        version: versionsModel.debian
                    }

                    VersionRow {
                        visible: versionsModel.systemAvailable && versionsModel.kernel !== ""
                        name: qsTr("Kernel")
                        version: versionsModel.kernel
                    }

                    BodyText {
                        objectName: "stateMessage"
                        visible: versionsModel.loaded && versionsModel.message !== ""
                        font.weight: Font.Normal
                        color: versionsModel.failure === "unreachable" || versionsModel.failure === "unauthorized"
                               ? Theme.redColor : Theme.blueColor
                        text: versionsModel.message
                    }

                    Text {
                        visible: page.packagesListed
                        text: qsTr("Packages")
                        font { family: calaosFont.fontFamily; weight: Font.Thin; pixelSize: Units.dp(18) }
                        color: Theme.whiteColor
                        Layout.topMargin: Units.dp(10)
                    }

                    Repeater {
                        model: versionsModel.systemAvailable ? versionsModel.packages : []
                        delegate: VersionRow {
                            required property var modelData
                            name: modelData.name
                            version: modelData.version
                        }
                    }
                }
            }
        }
    }

    ConfigTabs {
        id: tabs
        width: Units.dp(300)
        showVersions: false

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
        }
    }
}
