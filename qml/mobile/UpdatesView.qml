import QtQuick
import QtQuick.Controls as QuickControls
import QtQuick.Layouts
import SharedComponents

//Box updates from the phone, with the admin password (calaos_server admin
//relay). Same information as the webapp's update panel: one headline, the
//action it calls for, the progress fed by admin_event.
Item {
    id: page

    readonly property string gate: boxUpdates.gate
    readonly property string phase: boxUpdates.phase
    readonly property bool blocked: gate === "unavailable" || gate === "too_old" ||
                                    gate === "unreachable"
    readonly property bool gateScreen: gate === "https_required" || gate === "admin_not_set" ||
                                       gate === "old_server" || gate === "websocket_required" || blocked

    //A password typed for one screen never waits on another one, and a
    //reboot question never outlives the session that asked it.
    onGateChanged: {
        if (gate !== "locked")
            clearPassword()
        if (gate !== "unlocked")
            rebootSheet.close()
    }
    //a run started meanwhile (kiosk, webapp): the box refuses the reboot anyway
    onPhaseChanged: {
        if (phase === "upgrading")
            rebootSheet.close()
    }

    function clearPassword() {
        // qmllint disable missing-property
        Qt.inputMethod.reset()
        // qmllint enable missing-property
        passwordField.text = ""
    }

    function submitPassword() {
        if (passwordField.text === "" || boxUpdates.busy !== "" || boxUpdates.retryAfter > 0)
            return
        // qmllint disable missing-property
        Qt.inputMethod.commit()
        // qmllint enable missing-property
        const pass = passwordField.text
        const keep = keepPassword.visible && keepPassword.checked
        clearPassword()
        passwordField.focus = false
        if (keep)
            adminBiometrics.unlockAndKeep(pass)
        else
            boxUpdates.unlock(pass)
    }

    QuickControls.StackView.onActivated: boxUpdates.activate()
    QuickControls.StackView.onDeactivating: {
        clearPassword()
        rebootSheet.close()
        boxUpdates.deactivate()
    }
    Component.onDestruction: boxUpdates.deactivate()

    //Nothing typed stays in the field once the app leaves the screen.
    Connections {
        target: Qt.application
        function onStateChanged() {
            // qmllint disable missing-property
            if (Qt.application.state !== Qt.ApplicationActive)
                page.clearPassword()
            // qmllint enable missing-property
        }
    }

    component Title: Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.whiteColor
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(20) }
    }

    component Hint: Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(14) }
    }

    component Eyebrow: Text {
        Layout.fillWidth: true
        color: Theme.mutedColor
        elide: Text.ElideRight
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(11); capitalization: Font.AllUppercase }
    }

    component ErrorLine: Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.redColor
        visible: text !== ""
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
    }

    component Button: SettingsButton {
        Layout.fillWidth: true
        Layout.preferredHeight: Units.dp(52)
    }

    component Card: Rectangle {
        default property alias cardContent: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + Units.dp(32)
        color: Theme.colorAlpha(Theme.backgroundColor, 0.85)
        radius: Units.dp(12)
        border { color: "#333333"; width: 1 }

        ColumnLayout {
            id: cardColumn
            anchors {
                left: parent.left; right: parent.right; top: parent.top
                margins: Units.dp(16)
            }
            spacing: Units.dp(8)
        }
    }

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: listFlick

        y: header.height
        width: parent.width
        height: parent.height - header.height
        contentWidth: width
        contentHeight: content.implicitHeight + Units.dp(32)
        flickableDirection: Flickable.VerticalFlick
        clip: true

        ColumnLayout {
            id: content
            x: Units.dp(12)
            y: Units.dp(12)
            width: listFlick.width - Units.dp(24)
            spacing: Units.dp(12)

            //First contact with the box
            Card {
                visible: page.gate === "idle" || page.gate === "probing"
                Hint {
                    objectName: "updatesProbing"
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Checking the box…")
                }
            }

            //What the box does not allow from here
            Card {
                objectName: "updatesGateCard"
                visible: page.gateScreen

                Title {
                    objectName: "updatesGateTitle"
                    color: page.gate === "old_server" || page.gate === "https_required" ||
                           page.gate === "websocket_required" ? Theme.amberColor : Theme.whiteColor
                    text: page.gate === "https_required" ? qsTr("Updates need an https connection to the box") :
                          page.gate === "websocket_required" ? qsTr("Updates need a WebSocket connection to the box") :
                          page.gate === "admin_not_set" ? qsTr("Set the admin password from the web app first") :
                          page.gate === "unavailable" ? qsTr("Administration unavailable") :
                          page.gate === "too_old" ? (boxUpdates.rebootHint ? qsTr("Restart the box")
                                                                           : qsTr("Administration is being activated")) :
                          page.gate === "unreachable" ? qsTr("The box is not answering") :
                          page.gate === "old_server" ? qsTr("calaos-server is too old") : ""
                }
                Hint {
                    text: page.gate === "https_required" ?
                              qsTr("Administration only works over a secure connection. Log in again with "
                                   + "the box address alone or one that starts with https://, not http://…:5454.") :
                          page.gate === "websocket_required" ?
                              qsTr("The app is connected to the box through its HTTP fallback, which cannot "
                                   + "carry administration. Log in again to retry the WebSocket connection.") :
                          page.gate === "admin_not_set" ?
                              qsTr("No admin password is set on this box yet. Open the Calaos web app on a "
                                   + "computer, choose the admin password in its Administration page, then "
                                   + "come back here.") :
                          page.gate === "unavailable" ?
                              qsTr("Administration from the app is only available on Calaos OS. Trying "
                                   + "again automatically.") :
                          page.gate === "too_old" ?
                              (boxUpdates.rebootHint ?
                                   qsTr("Activation is taking too long. Unplug the box and plug it back in, "
                                        + "or reboot it over SSH.") :
                                   qsTr("The box is finishing its update. This usually takes a minute; "
                                        + "trying again automatically.")) :
                          page.gate === "unreachable" ?
                              qsTr("Trying again automatically.") :
                          page.gate === "old_server" ?
                              qsTr("This box cannot be administered from the app yet. Update it once with "
                                   + "its usual update (the button on the touch screen, or calaos-os "
                                   + "upgrade); after that, updates run from here.") : ""
                }
                Button {
                    objectName: "updatesChangeHost"
                    visible: page.gate === "https_required"
                    text: qsTr("Change the box address")
                    onButtonClicked: calaosApp.userLogout()
                }
                Button {
                    objectName: "updatesLoginAgain"
                    visible: page.gate === "websocket_required"
                    text: qsTr("Log in again")
                    onButtonClicked: calaosApp.userLogout()
                }
                Button {
                    objectName: "updatesRetry"
                    visible: page.gate === "admin_not_set" || page.gate === "old_server" ||
                             page.gate === "https_required"
                    text: qsTr("Try again")
                    onButtonClicked: boxUpdates.retry()
                }
            }

            //The update state; without a session only the figures (household view)
            Card {
                objectName: "updatesCard"
                visible: page.gate === "unlocked" ||
                         (page.gate === "locked" && page.phase === "upgrading")

                Eyebrow {
                    text: boxUpdates.boxName !== "" ? qsTr("Updates · %1").arg(boxUpdates.boxName)
                                                    : qsTr("Updates")
                }
                Title {
                    objectName: "updatesHeadline"
                    color: page.phase === "upgrading" ? Theme.blueColor :
                           page.phase === "doneReboot" ? Theme.amberColor :
                           page.phase === "failed" ? Theme.redColor :
                           page.phase === "upToDate" || page.phase === "done" ? Theme.greenColor :
                                                                               Theme.whiteColor
                    text: page.phase === "available" ? qsTr("%n update(s) available", "", boxUpdates.updatesCount) :
                          page.phase === "upgrading" ? qsTr("Updating… %1 %").arg(boxUpdates.totalPercent) :
                          page.phase === "upToDate" ? qsTr("Your box is up to date") :
                          page.phase === "done" ? qsTr("Update finished") :
                          page.phase === "doneReboot" ? qsTr("Update finished, restart the box") :
                          page.phase === "failed" ? qsTr("The update failed") :
                                                    qsTr("Updates not checked yet")
                }
                Hint {
                    text: page.phase === "available" ?
                              qsTr("Installing takes a few minutes. The box stays usable; the app may lose "
                                   + "the connection for a moment and comes back on its own.") :
                          page.phase === "upgrading" ?
                              (boxUpdates.externalRun ? qsTr("An update started from elsewhere is running.") + " " : "") +
                              qsTr("Keep the box powered. The app may lose the connection for a moment while "
                                   + "calaos-server restarts; it comes back on its own.") :
                          page.phase === "upToDate" ? qsTr("Nothing to install.") :
                          page.phase === "done" ? qsTr("Everything installed. The box is ready.") :
                          page.phase === "doneReboot" ?
                              qsTr("Some of what was installed only takes effect after a restart.") :
                          page.phase === "failed" ? boxUpdates.failureText :
                              qsTr("Checking contacts the Calaos servers and can take up to a minute.")
                }
                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    elide: Text.ElideRight
                    color: Theme.whiteColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
                    text: page.phase !== "upgrading" || boxUpdates.pkgCount <= 0 || boxUpdates.pkgIndex <= 0 ? "" :
                          boxUpdates.currentPackage !== "" ?
                              qsTr("%1 (%2 of %3)").arg(boxUpdates.currentPackage)
                                                   .arg(boxUpdates.pkgIndex).arg(boxUpdates.pkgCount) :
                              qsTr("Package %1 of %2").arg(boxUpdates.pkgIndex).arg(boxUpdates.pkgCount)
                }

                Rectangle {
                    objectName: "updatesProgressBar"
                    visible: page.phase === "upgrading"
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(8)
                    Layout.preferredHeight: Units.dp(4)
                    radius: height / 2
                    color: Qt.rgba(1, 1, 1, 0.08)

                    Rectangle {
                        width: parent.width * boxUpdates.totalPercent / 100
                        height: parent.height
                        radius: parent.radius
                        color: Theme.blueColor
                        Behavior on width { NumberAnimation { duration: 600; easing.type: Easing.OutCubic } }
                    }
                }

                //Pending updates: calaos components one by one, the rest by group
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(4)
                    spacing: 0
                    visible: page.phase === "available" && page.gate === "unlocked"

                    Repeater {
                        model: boxUpdates.packagesModel

                        RowLayout {
                            id: pkgRow
                            required property string pkgName
                            required property string pkgInstalledVersion
                            required property string pkgNewVersion
                            required property bool pkgHasUpdate

                            visible: pkgHasUpdate
                            Layout.fillWidth: true
                            Layout.preferredHeight: Units.dp(32)
                            spacing: Units.dp(8)

                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: Theme.whiteColor
                                font { family: calaosFont.fontFamily; pixelSize: Units.dp(14) }
                                text: pkgRow.pkgName
                            }
                            Text {
                                color: Theme.mutedColor
                                font { family: calaosFont.fontFamily; pixelSize: Units.dp(12) }
                                text: qsTr("%1 → %2").arg(pkgRow.pkgInstalledVersion).arg(pkgRow.pkgNewVersion)
                            }
                        }
                    }

                    Repeater {
                        model: boxUpdates.groupsModel

                        RowLayout {
                            id: groupRow
                            required property string groupId
                            required property int groupPkgCount

                            Layout.fillWidth: true
                            Layout.preferredHeight: Units.dp(32)
                            spacing: Units.dp(8)

                            Text {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: Theme.whiteColor
                                font { family: calaosFont.fontFamily; pixelSize: Units.dp(14) }
                                text: groupRow.groupId === "kernel" ? qsTr("Linux kernel") : qsTr("System packages")
                            }
                            Text {
                                color: Theme.mutedColor
                                font { family: calaosFont.fontFamily; pixelSize: Units.dp(12) }
                                text: qsTr("%n package(s)", "", groupRow.groupPkgCount)
                            }
                        }
                    }
                }

                ErrorLine {
                    objectName: "updatesError"
                    visible: text !== "" && page.gate === "unlocked"
                    text: boxUpdates.errorText
                }

                Hint {
                    visible: boxUpdates.info === "reboot_sent"
                    color: Theme.blueColor
                    text: qsTr("Restarting the box… the app reconnects on its own.")
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(8)
                    spacing: Units.dp(8)
                    visible: page.gate === "unlocked" && page.phase !== "upgrading"

                    Button {
                        objectName: "updatesRebootNow"
                        visible: page.phase === "doneReboot"
                        disabled: boxUpdates.busy !== ""
                        text: qsTr("Restart the box")
                        onButtonClicked: rebootSheet.open()
                    }
                    Button {
                        objectName: "updatesUpgradeAll"
                        visible: page.phase === "available"
                        disabled: boxUpdates.busy !== ""
                        text: boxUpdates.busy === "update_upgrade_all" ? qsTr("Starting…")
                                                                         : qsTr("Update all")
                        onButtonClicked: boxUpdates.upgradeAll()
                    }
                    Button {
                        objectName: "updatesCheck"
                        disabled: boxUpdates.busy !== ""
                        text: boxUpdates.busy === "update_check" ? qsTr("Checking…")
                                                                   : qsTr("Check for updates")
                        onButtonClicked: boxUpdates.checkUpdates()
                    }
                }
            }

            //Restart, kept apart from the updates as in the webapp
            Card {
                visible: page.gate === "unlocked"

                Eyebrow { text: qsTr("Box") }
                Title { text: qsTr("Restart the box") }
                Hint { text: qsTr("Takes about a minute. Your lights and shutters keep their state.") }
                Button {
                    objectName: "updatesReboot"
                    disabled: boxUpdates.busy !== "" || page.phase === "upgrading"
                    text: qsTr("Reboot")
                    onButtonClicked: rebootSheet.open()
                }
            }

            //The admin password, asked again silently when the session ends
            Card {
                objectName: "updatesUnlockCard"
                visible: page.gate === "locked"

                Title { text: qsTr("Admin password") }
                Hint {
                    text: qsTr("Updates and restarts are protected by the admin password. It is not the "
                               + "one you sign in with.")
                }
                Hint {
                    visible: boxUpdates.info === "session_invalidated"
                    color: Theme.amberColor
                    text: qsTr("The admin password was just changed. Enter the new one.")
                }

                CalaosTextField {
                    id: passwordField
                    objectName: "adminPasswordField"
                    framed: true
                    Layout.fillWidth: true
                    Layout.preferredHeight: Units.dp(48)
                    placeholderText: qsTr("Admin password")
                    echoMode: TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    enabled: boxUpdates.busy === ""
                    EnterKey.type: Qt.EnterKeyGo
                    onAccepted: page.submitPassword()
                }

                CalaosCheck {
                    id: keepPassword
                    objectName: "keepPassword"
                    Layout.fillWidth: true
                    visible: adminBiometrics.available && !adminBiometrics.stored
                    enabled: boxUpdates.busy === ""
                    text: adminBiometrics.keepText
                }

                ErrorLine {
                    objectName: "unlockError"
                    text: boxUpdates.errorText
                }

                Button {
                    id: unlockButton
                    glyph: disabled ? "lock_off" : "lock"

                    objectName: "unlockButton"
                    disabled: passwordField.text === "" || boxUpdates.busy !== "" ||
                              boxUpdates.retryAfter > 0
                    text: boxUpdates.busy === "unlock" ? qsTr("Checking…") : qsTr("Unlock")
                    onButtonClicked: page.submitPassword()
                }
            }
        }
    }

    KeyboardScroller {
        flickable: listFlick
        revealTo: unlockButton
    }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
        headerLabel: qsTr("Box updates")
        glyph: "dns"
    }

    BottomSheet {
        id: rebootSheet
        objectName: "rebootSheet"
        fontFamily: calaosFont.fontFamily
        title: qsTr("Restart the box?")
        fitHeight: rebootColumn.implicitHeight

        ColumnLayout {
            id: rebootColumn
            width: parent.width
            spacing: Units.dp(12)

            Hint {
                color: Theme.whiteColor
                text: qsTr("The app reconnects on its own once the box is back.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(12)

                Button {
                    text: qsTr("Cancel")
                    onButtonClicked: rebootSheet.close()
                }
                Button {
                    objectName: "rebootConfirm"
                    disabled: page.phase === "upgrading" || boxUpdates.busy !== ""
                    text: qsTr("Restart")
                    onButtonClicked: {
                        rebootSheet.close()
                        boxUpdates.reboot()
                    }
                }
            }
        }
    }
}
