import QtQuick
import QtQuick.Layouts
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

    width: 0
    height: 0

    function openFor(io) {
        climate.modelData = io
        detail.show()
    }

    //The overlay layer only knows one current dialog: give it back to the
    //first one when the dialog opened above it is gone
    function resumeOverlay() {
        if (detail.showing && detail.parent && detail.parent.hasOwnProperty("currentOverlay"))
            detail.parent.currentOverlay = detail
    }

    ClimateModel { id: climate }

    Dialog {
        id: detail
        objectName: "detailDialog"

        title: climate.name
        minimumWidth: Units.dp(480)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")
        onClosed: climate.flush()

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
        objectName: "modeDialog"
        title: qsTr("Mode — %1").arg(climate.name)
        minimumWidth: Units.dp(400)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")
        onClosed: host.resumeOverlay()

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
        objectName: "moreDialog"
        title: qsTr("More settings — %1").arg(climate.name)
        minimumWidth: climate.caps.fanModes.length > 0 && climate.caps.swingModes.length > 0 ? Units.dp(560) : Units.dp(400)
        hasActions: true
        positiveButtonEnabled: false
        negativeButtonText: qsTr("Close")
        onClosed: host.resumeOverlay()

        ClimateMore {
            objectName: "more"
            width: parent.width
            columnCount: climate.caps.fanModes.length > 0 && climate.caps.swingModes.length > 0 ? 2 : 1
            climate: climate
            fontFamily: host.fontFamily
        }
    }
}
