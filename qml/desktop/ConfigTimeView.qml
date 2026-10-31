import QtQuick
import SharedComponents
import QtQuick.Layouts

PageScaffold {
    id: page

    title: qsTr("Date and time")

    //"main", "zones", "manual"
    property string pane: "main"

    component BodyText: Text {
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
        color: Theme.colorAlpha(Theme.whiteColor, 0.7)
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    component SettingRow: RowLayout {
        id: row

        property alias title: rowTitle.text
        property alias detail: rowDetail.text
        property alias action: rowAction.text
        property alias actionName: rowAction.objectName

        signal triggered()

        spacing: Units.dp(12)
        Layout.fillWidth: true

        ColumnLayout {
            spacing: Units.dp(2)
            Layout.fillWidth: true

            BodyText {
                id: rowTitle
                color: Theme.whiteColor
            }
            BodyText {
                id: rowDetail
                font.pixelSize: Units.dp(13)
                color: Theme.blueColor
            }
        }

        CalaosButton {
            id: rowAction
            disabled: boxTime.busy
            Layout.minimumWidth: width
            onButtonClicked: row.triggered()
        }
    }

    function ntpLabel() {
        switch (boxTime.ntpState) {
        case "synchronized": return qsTr("Set automatically (NTP), synchronized")
        case "notSynchronized": return qsTr("Set automatically (NTP), not yet synchronized")
        case "manual": return qsTr("Set by hand")
        }
        return ""
    }

    function ntpColor() {
        switch (boxTime.ntpState) {
        case "synchronized": return Theme.greenColor
        case "notSynchronized": return Theme.amberColor
        }
        return Theme.mutedColor
    }

    function ask(action, title, text, button) {
        confirm.action = action
        confirm.title = title
        confirm.text = text
        confirm.positiveButtonText = button
        confirm.show()
    }

    function askServerRestart() {
        ask("server", qsTr("Restart the Calaos server?"),
            qsTr("Home automation stops for a moment, then this page reconnects by itself."),
            qsTr("Restart the Calaos server"))
    }

    function askKioskRestart() {
        ask("kiosk", qsTr("Restart Calaos Home?"),
            boxTime.serverRestartDone?
                qsTr("This screen closes for a moment, then comes back by itself.") :
                qsTr("This screen closes for a moment, then comes back by itself. The Calaos server has not been restarted: do it first, this page will not offer it again."),
            qsTr("Restart Calaos Home"))
    }

    function setManualTime(wheels) {
        const res = boxTime.checkManualTime(wheels.year, wheels.month, wheels.day,
                                            wheels.hour, wheels.minute, wheels.second)
        if (!res.ok) {
            manualProblem = res.problem
        } else if (res.needsConfirm) {
            confirm.manualTime = res.time
            ask("manual", qsTr("Set the box time?"), res.confirmText, qsTr("Set the time"))
        } else {
            applyManualTime(res.time)
        }
    }

    function applyManualTime(time) {
        boxTime.applyManualTime(time)
        pane = "main"
    }

    function runConfirmed() {
        switch (confirm.action) {
        case "manual": applyManualTime(confirm.manualTime); break
        case "server": boxTime.restartServer(); break
        case "kiosk": calaosApp.restartApp(); break
        }
    }

    property string manualProblem: ""

    Component.onCompleted: boxTime.activate()
    //overlays do not follow the page out of the stack
    Component.onDestruction: {
        boxTime.deactivate()
        confirm.close()
    }

    footerButtons: [
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
                id: mainPane

                anchors.fill: parent
                visible: page.pane === "main"
                contentHeight: mainColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: mainColumn

                    width: mainPane.width - Units.dp(12)
                    spacing: Units.dp(14)

                    BodyText {
                        objectName: "stateLoading"
                        text: qsTr("Reading the clock of the box…")
                        visible: !boxTime.loaded && !boxTime.updateRequired && boxTime.loadError === ""
                    }

                    BodyText {
                        objectName: "stateUpdate"
                        color: Theme.blueColor
                        text: qsTr("This box must be updated to set its date and time from this screen. The rest of the configuration is not affected.")
                        visible: boxTime.updateRequired
                    }

                    RowLayout {
                        spacing: Units.dp(12)
                        visible: boxTime.loadError !== ""
                        Layout.fillWidth: true

                        BodyText {
                            objectName: "stateError"
                            color: Theme.redColor
                            text: boxTime.loadError
                        }

                        CalaosButton {
                            objectName: "retry"
                            text: qsTr("Try again")
                            disabled: boxTime.busy
                            Layout.minimumWidth: width
                            onButtonClicked: boxTime.refresh()
                        }
                    }

                    ColumnLayout {
                        spacing: Units.dp(2)
                        visible: boxTime.loaded
                        Layout.fillWidth: true

                        Text {
                            objectName: "clock"
                            text: boxTime.timeText
                            font { family: calaosFont.fontFamilyThin; weight: Font.ExtraLight; pixelSize: Units.dp(48) }
                            color: Theme.whiteColor
                            Layout.fillWidth: true
                        }

                        BodyText {
                            objectName: "date"
                            text: boxTime.dateText
                        }

                        RowLayout {
                            spacing: Units.dp(8)
                            Layout.fillWidth: true

                            Rectangle {
                                Layout.preferredWidth: Units.dp(10)
                                Layout.preferredHeight: Units.dp(10)
                                radius: Units.dp(5)
                                color: page.ntpColor()
                            }

                            BodyText {
                                objectName: "ntpState"
                                text: page.ntpLabel()
                            }
                        }
                    }

                    BodyText {
                        objectName: "busy"
                        color: Theme.blueColor
                        text: qsTr("Applying…")
                        visible: boxTime.busy
                    }

                    RowLayout {
                        spacing: Units.dp(12)
                        visible: boxTime.message !== ""
                        Layout.fillWidth: true

                        BodyText {
                            objectName: "message"
                            color: boxTime.messageIsError? Theme.redColor : Theme.blueColor
                            text: boxTime.message
                        }

                        CalaosButton {
                            objectName: "readAgain"
                            text: qsTr("Read again")
                            visible: boxTime.unconfirmed
                            disabled: boxTime.busy
                            Layout.minimumWidth: width
                            onButtonClicked: boxTime.refresh()
                        }
                    }

                    ColumnLayout {
                        objectName: "restartNotice"
                        spacing: Units.dp(6)
                        visible: boxTime.restartRequired
                        Layout.fillWidth: true

                        BodyText {
                            color: Theme.blueColor
                            text: qsTr("Restart to use the new time zone")
                        }

                        BodyText {
                            font.pixelSize: Units.dp(13)
                            text: qsTr("The Calaos server and this screen read the time zone when they start.")
                        }

                        Flow {
                            spacing: Units.dp(8)
                            Layout.fillWidth: true

                            CalaosButton {
                                objectName: "restartServer"
                                text: qsTr("Restart the Calaos server")
                                visible: !boxTime.serverRestartDone
                                disabled: boxTime.busy
                                onButtonClicked: page.askServerRestart()
                            }

                            CalaosButton {
                                objectName: "restartKiosk"
                                text: qsTr("Restart Calaos Home")
                                onButtonClicked: page.askKioskRestart()
                            }

                            CalaosButton {
                                objectName: "restartLater"
                                text: qsTr("Later")
                                onButtonClicked: boxTime.dismissRestart()
                            }
                        }
                    }

                    SettingRow {
                        visible: boxTime.loaded
                        title: qsTr("Time zone")
                        detail: boxTime.timezone !== ""? boxTime.timezone : qsTr("No time zone")
                        action: qsTr("Change")
                        actionName: "changeZone"
                        onTriggered: page.pane = "zones"
                    }

                    SettingRow {
                        visible: boxTime.loaded
                        title: qsTr("Automatic time (NTP)")
                        detail: qsTr("The box sets its clock from time servers on the internet.")
                        action: boxTime.ntp? qsTr("Stop using NTP") : qsTr("Use NTP")
                        actionName: "toggleNtp"
                        onTriggered: boxTime.setNtp(!boxTime.ntp)
                    }

                    SettingRow {
                        visible: boxTime.loaded
                        title: qsTr("Set the date and time")
                        detail: qsTr("In the time zone of the box. Setting it by hand turns automatic time off.")
                        action: qsTr("Set")
                        actionName: "setManual"
                        onTriggered: {
                            page.manualProblem = ""
                            page.pane = "manual"
                        }
                    }
                }
            }

            ScrollBar {
                listObject: mainPane
                visible: mainPane.visible
            }

            Loader {
                anchors.fill: parent
                active: page.pane === "zones"

                sourceComponent: Component {
                    TimeZonePicker {
                        objectName: "zonePicker"
                        onChosen: function(zone) {
                            boxTime.setTimezone(zone)
                            page.pane = "main"
                        }
                        onCancelled: page.pane = "main"
                    }
                }
            }

            Loader {
                anchors.fill: parent
                active: page.pane === "manual"

                sourceComponent: Component {
                    ColumnLayout {
                        spacing: Units.dp(10)

                        BodyText {
                            text: qsTr("In the time zone of the box (%1).").arg(boxTime.timezone !== ""? boxTime.timezone : "UTC")
                        }

                        DateTimeWheels {
                            id: wheels
                            objectName: "wheels"
                            Layout.fillWidth: true
                            Component.onCompleted: setValues(boxTime.localNow())
                            onPickedChanged: page.manualProblem = ""
                        }

                        BodyText {
                            objectName: "manualProblem"
                            color: Theme.redColor
                            text: page.manualProblem
                            visible: text !== ""
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            spacing: Units.dp(8)
                            Layout.fillWidth: true

                            Item { height: 1; Layout.fillWidth: true }

                            CalaosButton {
                                objectName: "manualCancel"
                                text: qsTr("Cancel")
                                Layout.minimumWidth: width
                                onButtonClicked: page.pane = "main"
                            }

                            CalaosButton {
                                objectName: "manualSet"
                                text: qsTr("Set")
                                disabled: !boxTime.loaded || boxTime.busy
                                Layout.minimumWidth: width
                                onButtonClicked: page.setManualTime(wheels)
                            }
                        }
                    }
                }
            }
        }
    }

    ConfigTabs {
        id: tabs
        width: Units.dp(300)

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
        }
    }

    Dialog {
        id: confirm
        objectName: "confirm"

        property string action: ""
        property string manualTime: ""

        onAccepted: page.runConfirmed()
    }
}
