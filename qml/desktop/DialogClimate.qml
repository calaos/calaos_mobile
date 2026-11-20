import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import SharedComponents
import "../SharedComponents/ClimateLogic.js" as Logic

//Settings of a climate device on the desktop: a dialog with the dial, the
//setpoint line, the mode pill and "More settings". The mode choice and the
//more settings are dialogs of their own, opened on top of this one. They are
//siblings, not children: a dialog moves itself onto the overlay layer when it
//opens, and one nested in another does not get drawn there.
Item {
    id: host

    property string fontFamily: calaosFont.fontFamily
    property alias clock: climate.clock
    readonly property alias climate: climate
    readonly property alias detail: detail
    readonly property alias modeDialog: modeDialog
    readonly property alias moreDialog: moreDialog
    readonly property alias programDialog: programDialog

    width: 0
    height: 0

    function openFor(io) {
        climate.modelData = io
        detail.show()
    }

    ClimateModel { id: climate }

    //Let go of the device once the dialog has faded out: nothing keeps counting or listening
    Timer {
        id: release
        interval: 400
        onTriggered: {
            if (!detail.showing)
                climate.modelData = null
        }
    }

    Dialog {
        id: detail
        closeOnDisconnect: true
        objectName: "detailDialog"

        title: climate.name
        minimumWidth: Units.dp(480)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")
        //The dialogs opened on top of this one leave a band of it uncovered: it
        //must not take clicks, or its Close would close this one under them
        enabled: !modeDialog.showing && !moreDialog.showing && !programDialog.showing
        onClosed: {
            climate.flush()
            release.restart()
        }

        ClimateDetail {
            objectName: "detail"
            anchors.horizontalCenter: parent.horizontalCenter
            climate: climate
            fontFamily: host.fontFamily
            dialSize: Units.dp(240)
            numSize: Units.dp(48)
        }

        ClimateModePill {
            objectName: "modePill"
            anchors.horizontalCenter: parent.horizontalCenter
            height: Units.dp(40)
            fontFamily: host.fontFamily
            fontSize: Units.dp(16)
            iconSize: Units.dp(18)
            word: climate.mode !== "" ? climate.modeLabel(climate.mode) : qsTr("Mode unknown")
            icon: climate.modeIcon(climate.mode)
            disabled: climate.status.kind === "offline" || climate.caps.modes.length === 0
            onButtonClicked: modeDialog.show()
        }

        ClimateProgramCard {
            objectName: "programCard"
            width: parent.width
            climate: climate
            fontFamily: host.fontFamily
            onEditRequested: programDialog.show()
        }

        MouseArea {
            objectName: "moreLink"
            anchors.horizontalCenter: parent.horizontalCenter
            width: moreText.implicitWidth + Units.dp(30)
            height: Units.dp(40)
            visible: Logic.hasMore(climate.caps, climate.devState)
            onClicked: moreDialog.show()

            Text {
                id: moreText
                anchors { verticalCenter: parent.verticalCenter; left: parent.left }
                color: Theme.mutedColor
                font { family: host.fontFamily; weight: Font.Light; pixelSize: Units.dp(15) }
                text: qsTr("More settings")
            }

            ClimateIcon {
                anchors { verticalCenter: parent.verticalCenter; right: parent.right }
                name: "chevronR"
                size: Units.dp(18)
                color: Theme.mutedColor
            }
        }
    }

    Dialog {
        id: modeDialog
        closeOnDisconnect: true
        objectName: "modeDialog"
        title: qsTr("Mode — %1").arg(climate.name)
        minimumWidth: Units.dp(400)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")

        ClimateRows {
            objectName: "modeRows"
            width: parent.width
            fontFamily: host.fontFamily
            choices: climate.caps.modes.map(function(m) {
                return { value: m, label: climate.modeLabel(m), icon: climate.modeIcon(m) }
            })
            current: climate.mode
            onChosen: function(value) {
                climate.chooseMode(value)
                modeDialog.close()
            }
        }
    }

    Dialog {
        id: moreDialog
        closeOnDisconnect: true
        objectName: "moreDialog"
        title: qsTr("More settings — %1").arg(climate.name)
        minimumWidth: climate.caps.fanModes.length > 0 && climate.caps.swingModes.length > 0 ? Units.dp(560) : Units.dp(400)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")

        ClimateMore {
            objectName: "more"
            width: parent.width
            columnCount: climate.caps.fanModes.length > 0 && climate.caps.swingModes.length > 0 ? 2 : 1
            climate: climate
            fontFamily: host.fontFamily
        }
    }

    //Near full screen, one page at a time: the editor carries its own header
    //and close button. Dialog.qml cannot grow that tall.
    PopupBase {
        id: programDialog
        objectName: "programDialog"
        closeOnDisconnect: true
        overlayLayer: "dialogOverlayLayer"
        overlayColor: Qt.rgba(0, 0, 0, 0.6)
        dismissOnTap: false
        anchors { fill: parent; margins: Units.dp(18) }
        opacity: showing ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: Theme.reduceMotion ? 0 : 200 }
        }

        onOpened: editor.forceActiveFocus()
        onClosed: editor.home()

        function show() {
            open()
        }

        //The editor takes Back / Escape while it can go back a page.
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Escape && showing)
                event.accepted = true
        }
        Keys.onReleased: (event) => {
            if ((event.key === Qt.Key_Escape || event.key === Qt.Key_Back) && showing) {
                close()
                event.accepted = true
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.backgroundColor
            border.color: Theme.borderColor
            radius: Units.dp(8)

            //Taps on the dialog must not reach the veil.
            MouseArea { anchors.fill: parent }
        }

        TimeRangeEditor {
            id: editor
            objectName: "programEditor"
            anchors { fill: parent; margins: Units.dp(14) }
            fontFamily: host.fontFamily
            device: climate.modelData ? climate.modelData.device : ({})
            readOnly: climate.status.kind === "offline"
            title: climate.name
            holdSetpoint: climate.values ? climate.values.target : undefined
            onCloseRequested: programDialog.close()

            Connections {
                target: climate.modelData ? climate.modelData : null
                ignoreUnknownSignals: true
                function onDeviceChange() { editor.refreshStates() }
            }
        }
    }
}
