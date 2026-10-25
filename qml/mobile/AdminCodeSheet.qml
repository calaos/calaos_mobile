pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import SharedComponents

//Admin code pushed to this phone, decrypted by the app (AdminCodeModel): the
//notification itself only says a code is waiting. Shown for a minute from
//its reception, newest first; closing the sheet consumes every code shown.
//Stays open across logins: reading a code needs no connection to the box.
Item {
    id: host

    property string fontFamily: calaosFont.fontFamily
    property var codeModel: typeof adminCodeModel !== "undefined" ? adminCodeModel : null
    readonly property var codes: codeModel ? codeModel.codes : []
    readonly property int lifetime: codeModel ? codeModel.lifetime : 60
    readonly property bool opened: sheet.opened
    readonly property bool shown: sheet.visible

    width: 0
    height: 0

    function purpose(action) {
        switch (action) {
        case "console_open": return qsTr("To open the web console")
        case "admin_password": return qsTr("To change the admin password")
        case "approver_enroll": return qsTr("To add an approver phone")
        case "approver_remove": return qsTr("To remove an approver phone")
        case "approver_confirm": return qsTr("To enroll this phone as the admin approver")
        case "network_config": return qsTr("To change the network configuration")
        }
        return ""
    }

    Connections {
        target: host.codeModel
        ignoreUnknownSignals: true
        function onShowRequested() { sheet.open() }
    }

    BottomSheet {
        id: sheet
        objectName: "adminCodeSheet"
        closeOnLogout: false
        fontFamily: host.fontFamily
        title: qsTr("Admin code")
        fitHeight: column.implicitHeight
        onClosed: {
            if (host.codeModel)
                host.codeModel.close()
        }

        SheetScroll {
            anchors.fill: parent
            implicitContentHeight: column.implicitHeight

            ColumnLayout {
                id: column
                width: parent.width
                spacing: Units.dp(8)

                Repeater {
                    objectName: "adminCodeList"
                    //By count: the list is a new value every second, the
                    //delegates (and the bar animation) must survive it.
                    model: host.codes.length

                    delegate: ColumnLayout {
                        id: entry
                        required property int index
                        readonly property var modelData: host.codes[index] || ({})
                        objectName: "adminCode_" + modelData.codeId
                        readonly property bool expired: modelData.expired === true

                        Layout.fillWidth: true
                        Layout.topMargin: index > 0 ? Units.dp(12) : 0
                        spacing: Units.dp(8)

                        Rectangle {
                            visible: entry.index > 0
                            Layout.fillWidth: true
                            Layout.bottomMargin: Units.dp(4)
                            Layout.preferredHeight: 1
                            color: Theme.colorAlpha(Theme.whiteColor, 0.12)
                        }

                        Text {
                            objectName: "adminCodePurpose"
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            color: Theme.whiteColor
                            font { family: host.fontFamily; pixelSize: Units.dp(14) }
                            text: host.purpose(entry.modelData.action)
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Units.dp(56)

                            //Fixed cells: the digits do not move as they are read.
                            Row {
                                objectName: "adminCodeDigits"
                                anchors.centerIn: parent
                                visible: !entry.expired
                                spacing: Units.dp(4)

                                Repeater {
                                    model: entry.expired ? [] : String(entry.modelData.code || "").split("")
                                    delegate: Text {
                                        required property string modelData
                                        required property int index
                                        width: Units.dp(30) + (index === 2 ? Units.dp(14) : 0)
                                        horizontalAlignment: Text.AlignLeft
                                        color: Theme.blueColor
                                        font { family: host.fontFamily; pixelSize: Units.dp(44); bold: true }
                                        text: modelData
                                    }
                                }
                            }

                            Text {
                                objectName: "adminCodeExpired"
                                anchors.centerIn: parent
                                visible: entry.expired
                                color: Theme.mutedColor
                                font { family: host.fontFamily; pixelSize: Units.dp(28) }
                                text: qsTr("Expired")
                            }
                        }

                        Rectangle {
                            id: track
                            Layout.fillWidth: true
                            Layout.preferredHeight: Units.dp(4)
                            radius: height / 2
                            color: Theme.colorAlpha(Theme.whiteColor, 0.12)

                            Rectangle {
                                objectName: "adminCodeBar"
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                                radius: parent.radius
                                width: track.width * Math.max(0, Math.min(1, entry.modelData.fraction || 0))
                                color: entry.modelData.remaining <= 10 ? Theme.amberColor : Theme.blueColor
                                //The model moves a step per second: drain smoothly in between.
                                Behavior on width { NumberAnimation { duration: 1000 } }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Units.dp(12)

                            Text {
                                objectName: "adminCodeCountdown"
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: entry.expired ? Theme.mutedColor : Theme.whiteColor
                                font { family: host.fontFamily; pixelSize: Units.dp(13) }
                                text: entry.expired ? qsTr("Ask the web console for a new code.")
                                                    : qsTr("Valid for %1 s").arg(entry.modelData.remaining)
                            }

                            CalaosItemBase {
                                objectName: "adminCodeCopy"
                                Layout.preferredWidth: Units.dp(110)
                                Layout.preferredHeight: Units.dp(36)
                                visible: !entry.expired
                                text: qsTr("Copy")
                                onButtonClicked: host.codeModel.copy(entry.modelData.codeId)
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(8)
                    wrapMode: Text.Wrap
                    color: Theme.mutedColor
                    font { family: host.fontFamily; pixelSize: Units.dp(13) }
                    text: qsTr("Type it in the Calaos web console. Never give it to anyone.")
                }

                CalaosItemBase {
                    objectName: "adminCodeClose"
                    Layout.fillWidth: true
                    Layout.topMargin: Units.dp(4)
                    Layout.preferredHeight: Units.dp(40)
                    text: qsTr("Close")
                    onButtonClicked: sheet.close()
                }
            }
        }
    }
}
