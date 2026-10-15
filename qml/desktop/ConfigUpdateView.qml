import QtQuick
import SharedComponents
import QtQuick.Layouts

Item {

    //identifies this page on the navigation stack (see main.qml)
    property string viewId: "config/update"

    function pkgStatusText(hasUpdate, upState, percent) {
        if (upState === "done") return qsTr("Update installed")
        if (upState === "failed") return qsTr("Update failed")
        if (upState === "downloading") return qsTr("Downloading") + " " + percent + "%"
        if (upState === "unpacking") return qsTr("Unpacking") + " " + percent + "%"
        if (upState === "installing") return qsTr("Installing") + " " + percent + "%"
        if (upState === "pending") return qsTr("Waiting")
        return hasUpdate ? qsTr("Update available") : qsTr("Up to date")
    }

    function pkgStatusColor(hasUpdate, upState) {
        if (upState === "done") return Theme.greenColor
        if (upState === "failed") return Theme.redColor
        if (upState === "downloading" || upState === "unpacking" || upState === "installing")
            return Theme.blueColor
        if (upState === "pending") return Theme.colorAlpha(Theme.whiteColor, 0.4)
        return hasUpdate ? Theme.yellowColor : Theme.greenColor
    }

    function groupTitle(id) {
        if (id === "kernel") return qsTr("Linux kernel")
        if (id === "system") return qsTr("System packages")
        return id
    }

    //Section header: ExtraLight title followed by a hairline rule
    component SectionHeader: Item {
        property alias title: sectionText.text

        width: parent.width
        height: Units.dp(34)

        Text {
            id: sectionText
            font { family: calaosFont.fontFamilyLight; weight: Font.ExtraLight; pixelSize: Units.dp(20) }
            color: Theme.colorAlpha(Theme.whiteColor, 0.85)

            anchors {
                left: parent.left
                bottom: parent.bottom; bottomMargin: Units.dp(4)
            }
        }

        Rectangle {
            height: Units.dp(1)
            color: Theme.colorAlpha(Theme.whiteColor, 0.12)

            anchors {
                left: sectionText.right; leftMargin: Units.dp(14)
                right: parent.right
                verticalCenter: sectionText.verticalCenter
            }
        }
    }

    //Small round status indicator, like a hardware LED
    component StatusDot: Rectangle {
        implicitWidth: Units.dp(9)
        implicitHeight: Units.dp(9)
        radius: width / 2
    }

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footer.top
            left: footer.left
            right: footer.right
        }
        opacity: 0.6
    }

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: tabs.left; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }

            clip: true

            //Normal state: browsable update list
            Item {
                anchors.fill: parent
                visible: !updateManager.isUpgrading

                Flickable {
                    id: flick

                    anchors {
                        fill: parent
                        topMargin: Units.dp(3)
                        bottomMargin: Units.dp(3)
                        leftMargin: Units.dp(5)
                        rightMargin: Units.dp(5)
                    }

                    contentWidth: width
                    contentHeight: contentCol.implicitHeight + Units.dp(24)
                    boundsBehavior: Flickable.StopAtBounds

                    opacity: updateManager.isChecking ? 0.15 : 1
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    Column {
                        id: contentCol
                        x: Units.dp(8)
                        y: Units.dp(10)
                        width: flick.width - Units.dp(16)
                        spacing: Units.dp(16)

                        //Error banner, non blocking
                        Item {
                            width: parent.width
                            visible: !updateManager.daemonAvailable || updateManager.updateState === "error"
                            height: errorText.implicitHeight + Units.dp(12)

                            Rectangle {
                                id: errorBar
                                width: Units.dp(3)
                                radius: width / 2
                                color: Theme.redColor

                                anchors {
                                    left: parent.left
                                    top: parent.top; topMargin: Units.dp(2)
                                    bottom: parent.bottom; bottomMargin: Units.dp(2)
                                }
                            }

                            Text {
                                id: errorText
                                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(13) }
                                color: Theme.colorAlpha(Theme.redColor, 0.9)
                                wrapMode: Text.WordWrap
                                text: updateManager.lastErrorString !== "" ?
                                          updateManager.lastErrorString:
                                          qsTr("Cannot contact the update service")

                                anchors {
                                    left: errorBar.right; leftMargin: Units.dp(12)
                                    right: errorClose.left; rightMargin: Units.dp(12)
                                    verticalCenter: parent.verticalCenter
                                }
                            }

                            //dismiss only clears the error state: while the
                            //daemon is unreachable the banner stays visible
                            Text {
                                id: errorClose
                                visible: updateManager.updateState === "error"
                                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
                                color: Theme.colorAlpha(Theme.whiteColor, 0.5)
                                text: "✕"

                                anchors {
                                    right: parent.right; rightMargin: Units.dp(6)
                                    verticalCenter: parent.verticalCenter
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -Units.dp(10)
                                    onClicked: updateManager.clearError()
                                }
                            }
                        }

                        //Positive state: everything is current
                        Item {
                            width: parent.width
                            height: Units.dp(44)
                            visible: !updateManager.hasUpdates && updateManager.daemonAvailable && !updateManager.isChecking

                            Row {
                                anchors.centerIn: parent
                                spacing: Units.dp(10)

                                StatusDot {
                                    color: Theme.greenColor
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(17) }
                                    color: Theme.colorAlpha(Theme.whiteColor, 0.8)
                                    text: qsTr("Your system is up to date")
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }

                        //Calaos components section
                        Column {
                            width: parent.width
                            spacing: Units.dp(10)

                            SectionHeader {
                                title: qsTr("Calaos components")
                            }

                            Text {
                                visible: !updateManager.installedListAvailable && calaosRepeater.count === 0
                                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(13) }
                                color: Theme.colorAlpha(Theme.whiteColor, 0.4)
                                text: qsTr("Component list unavailable")
                            }

                            Repeater {
                                id: calaosRepeater
                                model: updateCalaosModel

                                delegate: ItemBase {
                                    height: pkgCol.implicitHeight + Units.dp(14)

                                    ColumnLayout {
                                        id: pkgCol
                                        spacing: Units.dp(3)

                                        anchors {
                                            left: parent.left; leftMargin: Units.dp(14)
                                            right: parent.right; rightMargin: Units.dp(14)
                                            verticalCenter: parent.verticalCenter
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Units.dp(10)

                                            StatusDot {
                                                Layout.alignment: Qt.AlignVCenter
                                                color: pkgStatusColor(pkgHasUpdate, pkgUpgradeState)
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                font { family: calaosFont.fontFamily; weight: Font.Medium; pixelSize: Units.dp(16) }
                                                color: Theme.colorAlpha(Theme.whiteColor, 0.85)
                                                text: pkgName
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignVCenter
                                                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(14) }
                                                color: Theme.colorAlpha(Theme.whiteColor, 0.4)
                                                text: pkgInstalledVersion
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignVCenter
                                                visible: pkgHasUpdate
                                                font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(14) }
                                                color: Theme.colorAlpha(Theme.whiteColor, 0.3)
                                                text: "→"
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignVCenter
                                                visible: pkgHasUpdate
                                                font { family: calaosFont.fontFamily; weight: Font.Medium; pixelSize: Units.dp(15) }
                                                color: Theme.blueColor
                                                text: pkgNewVersion
                                            }

                                            ItemButtonAction {
                                                Layout.alignment: Qt.AlignVCenter
                                                visible: pkgHasUpdate
                                                enabled: !updateManager.isUpgrading && !updateManager.isChecking
                                                opacity: enabled ? 1 : 0.4
                                                imageSource: "button_empty"
                                                iconSource: "qrc:/img/down_arrow.svg"
                                                onButtonClicked: updateManager.upgradePackage(pkgName)
                                            }
                                        }

                                        Text {
                                            Layout.leftMargin: Units.dp(19)
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(12) }
                                            color: Theme.colorAlpha(pkgStatusColor(pkgHasUpdate, pkgUpgradeState), 0.85)
                                            text: pkgStatusText(pkgHasUpdate, pkgUpgradeState, pkgPercent)
                                        }
                                    }
                                }
                            }
                        }

                        //System updates section
                        Column {
                            width: parent.width
                            spacing: Units.dp(10)
                            visible: groupsRepeater.count > 0

                            SectionHeader {
                                title: qsTr("System updates")
                            }

                            Repeater {
                                id: groupsRepeater
                                model: updateGroupsModel

                                delegate: ItemBase {
                                    height: groupCol.implicitHeight + Units.dp(14)

                                    ColumnLayout {
                                        id: groupCol
                                        spacing: Units.dp(3)

                                        anchors {
                                            left: parent.left; leftMargin: Units.dp(14)
                                            right: parent.right; rightMargin: Units.dp(14)
                                            verticalCenter: parent.verticalCenter
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Units.dp(10)

                                            StatusDot {
                                                Layout.alignment: Qt.AlignVCenter
                                                color: pkgStatusColor(true, groupUpgradeState)
                                            }

                                            Text {
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                                font { family: calaosFont.fontFamily; weight: Font.Medium; pixelSize: Units.dp(16) }
                                                color: Theme.colorAlpha(Theme.whiteColor, 0.85)
                                                text: groupTitle(groupId)
                                            }

                                            ItemButtonAction {
                                                Layout.alignment: Qt.AlignVCenter
                                                enabled: !updateManager.isUpgrading && !updateManager.isChecking
                                                opacity: enabled ? 1 : 0.4
                                                imageSource: "button_empty"
                                                iconSource: "qrc:/img/down_arrow.svg"
                                                onButtonClicked: updateManager.upgradeGroup(groupId)
                                            }
                                        }

                                        Text {
                                            Layout.leftMargin: Units.dp(19)
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            font { family: calaosFont.fontFamily; weight: Font.Light; pixelSize: Units.dp(12) }
                                            color: Theme.colorAlpha(Theme.whiteColor, 0.4)
                                            text: qsTr("%n package(s)", "", groupPkgCount) +
                                                  (groupPackages.length > 0 ? " — " + groupPackages.join(", ") : "")
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollBar { listObject: flick }

                //Checking overlay
                Item {
                    id: checkingOverlay
                    anchors.fill: parent
                    opacity: updateManager.isChecking ? 1 : 0
                    visible: opacity > 0
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    Column {
                        anchors.centerIn: parent
                        spacing: Units.dp(14)

                        Image {
                            source: "qrc:/img/spinner.svg"
                            width: Units.dp(42)
                            height: Units.dp(42)
                            sourceSize.width: Units.dp(42)
                            sourceSize.height: Units.dp(42)
                            opacity: 0.7
                            anchors.horizontalCenter: parent.horizontalCenter

                            RotationAnimator on rotation {
                                running: checkingOverlay.visible
                                loops: Animation.Infinite
                                duration: 2000
                                from: 0; to: 360
                            }
                        }

                        Text {
                            font { family: calaosFont.fontFamilyLight; weight: Font.ExtraLight; pixelSize: Units.dp(16) }
                            color: Theme.colorAlpha(Theme.whiteColor, 0.75)
                            text: qsTr("Checking for updates…")
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }

            //Upgrading state: live progress panel
            UpdateProgressPanel {
                anchors.fill: parent
                visible: updateManager.isUpgrading
            }
        }

    }

    Dialog {
        id: confirmDialog
        title: qsTr("Install all updates")
        text: qsTr("All available updates will be installed. This may take several minutes.")
        positiveButtonText: qsTr("Install")
        onAccepted: updateManager.upgradeAll()
    }

    ConfigTabs {
        id: tabs
        width: Units.dp(300)

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }
    }

    Image {
        id: header
        source: "qrc:/img/module_header.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Text {
            anchors.centerIn: parent
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(22)
            font.family: calaosFont.fontFamilyLight
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
            text: qsTr("Update your system")
        }
    }

    Image {
        id: footer
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: qsTr("Refresh")
                icon: "qrc:/img/button_action_reload.png"
                Layout.minimumWidth: width
                enabled: !updateManager.isChecking && !updateManager.isUpgrading
                opacity: enabled ? 1 : 0.5
                onBtClicked: updateManager.refresh()
            }

            FooterButton {
                label: qsTr("Update all (%1)").arg(updateManager.updatesCount)
                icon: "qrc:/img/button_action_valid.png"
                Layout.minimumWidth: width
                visible: updateManager.hasUpdates
                enabled: !updateManager.isChecking && !updateManager.isUpgrading
                opacity: enabled ? 1 : 0.5
                onBtClicked: confirmDialog.show()
            }

            FooterButton {
                label: qsTr("Back to config")
                icon: "qrc:/img/button_action_back.png"
                Layout.minimumWidth: width
                enabled: !updateManager.isUpgrading
                onBtClicked: rootWindow.handleBack()
            }

            FooterButton {
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                enabled: !updateManager.isUpgrading
                onBtClicked: rootWindow.goToDesktop()
            }
        }

    }
}
