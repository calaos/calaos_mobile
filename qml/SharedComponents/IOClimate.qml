import QtQuick
import QtQuick.Layouts
import "."
import "ClimateLogic.js" as Logic

//Climate device (spec 5.3, 6): everything it shows comes from the capabilities
//of the `device` object, so one widget serves every impl. The item's scalar
//state is the mode; "" means the mode is not known.
ItemBase {
    id: root

    property variant modelData

    readonly property bool deviceKnown: modelData ? modelData.hasDevice : false
    readonly property var caps: Logic.capabilities(modelData ? modelData.deviceCapabilities : undefined)
    readonly property var devState: modelData ? modelData.deviceState : undefined
    readonly property string mode: Logic.currentMode(modelData ? modelData.stateString : undefined)
    readonly property bool controlsActive: Logic.controlsActive(mode)
    readonly property string setpointKind: Logic.setpointKind(mode, caps)
    readonly property string action: devState && typeof devState.action === "string" ? devState.action : ""

    //Setpoint edits wait here so that a drag or a few taps on +/- make one
    //command; what was sent is held in `echoed` until the server reports it,
    //otherwise the control would jump back to the old value in between.
    property var pending: ({})
    property var echoed: ({})

    //ItemBase is a plain row that switches itself off; this one has controls.
    enabled: true
    height: layout.implicitHeight + Units.dp(16)

    function shown(key) {
        return Logic.pick(key, [pending, echoed, devState])
    }

    function sendFields(fields) {
        echoed = Logic.merged(echoed, fields)
        echoTimer.restart()
        modelData.sendStringValue(Logic.partialCommand(fields))
    }

    function queue(fields) {
        pending = Logic.merged(pending, fields)
        flushTimer.restart()
    }

    function flush() {
        if (!Logic.hasFields(pending))
            return
        var fields = pending
        pending = ({})
        sendFields(fields)
    }

    function chooseMode(value) {
        var cmd = Logic.modeCommand(value, caps)
        if (cmd !== "" && cmd !== mode)
            modelData.sendStringValue(cmd)
    }

    function modeLabel(m) {
        switch (m) {
        case "off": return qsTr("Off")
        case "heat": return qsTr("Heat")
        case "cool": return qsTr("Cool")
        case "auto": return qsTr("Auto")
        case "dry": return qsTr("Dry")
        case "fan_only": return qsTr("Fan only")
        default: return humanize(m)
        }
    }

    function optionLabel(o) {
        switch (o) {
        case "off": return qsTr("Off")
        case "on": return qsTr("On")
        case "auto": return qsTr("Auto")
        case "low": return qsTr("Low")
        case "medium": return qsTr("Medium")
        case "high": return qsTr("High")
        case "quiet": return qsTr("Quiet")
        case "both": return qsTr("Both")
        case "vertical": return qsTr("Vertical")
        case "horizontal": return qsTr("Horizontal")
        default: return humanize(o)
        }
    }

    function actionLabel(a) {
        switch (a) {
        case "off": return qsTr("Off")
        case "idle": return qsTr("Idle")
        case "heating": return qsTr("Heating")
        case "cooling": return qsTr("Cooling")
        case "drying": return qsTr("Drying")
        case "fan": return qsTr("Fan")
        default: return humanize(a)
        }
    }

    function humanize(s) {
        var t = String(s).replace(/_/g, " ")
        return t.charAt(0).toUpperCase() + t.slice(1)
    }

    function toneColor(tone) {
        switch (tone) {
        case "warm": return Theme.redColor
        case "cold": return Theme.blueColor
        case "dry": return Theme.yellowColor
        case "fan": return Theme.greenColor
        default: return Theme.colorAlpha(Theme.whiteColor, 0.54)
        }
    }

    function toChoices(values, labeler) {
        return values.map(function(v) { return { value: v, label: labeler(v) } })
    }

    Timer {
        id: flushTimer
        interval: 400
        onTriggered: root.flush()
    }

    Timer {
        id: echoTimer
        interval: 2500
        onTriggered: root.echoed = ({})
    }

    Connections {
        target: root.modelData ? root.modelData : null

        function onDeviceChange() {
            root.echoed = Logic.confirmed(root.echoed, root.modelData.deviceState)
        }
    }

    ColumnLayout {
        id: layout

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            top: parent.top; topMargin: Units.dp(8)
        }
        spacing: Units.dp(6)

        RowLayout {
            Layout.fillWidth: true
            spacing: Units.dp(6)

            IconItem {
                source: calaosApp.getPictureSized("icon_temp")

                Layout.preferredHeight: height
                Layout.preferredWidth: width
            }

            ScrollingText {
                id: name
                color: Theme.blueColor
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                text: root.modelData ? root.modelData.ioName : ""
                clip: true
                Layout.fillWidth: true
            }

            //What the device is doing, or the reason nothing is selected
            Rectangle {
                id: badge
            objectName: "badge"

                readonly property bool unknownMode: root.deviceKnown && root.mode === ""
                readonly property bool shown: root.deviceKnown && (unknownMode ||
                    (root.caps.reportsAction && root.action !== ""))
                readonly property color tone: unknownMode ? root.toneColor("idle")
                                                          : root.toneColor(Logic.actionTone(root.action))

                visible: shown
                radius: Units.dp(4)
                color: Theme.colorAlpha(tone, 0.2)
                implicitWidth: badgeText.implicitWidth + Units.dp(12)
                implicitHeight: badgeText.implicitHeight + Units.dp(6)

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    color: badge.tone
                    font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 10 }
                    text: badge.unknownMode ? qsTr("Mode unknown") : root.actionLabel(root.action)
                }
            }

            Text {
                id: currentTemp
                objectName: "currentTemp"
                color: Theme.yellowColor
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                text: root.deviceKnown
                      ? Logic.formatReading(root.devState ? root.devState.current_temp : undefined,
                                            root.caps.bounds ? root.caps.bounds.unit : undefined)
                      : (root.mode !== "" ? root.modeLabel(root.mode) : Logic.ABSENT)
            }
        }

        ClimateChoiceRow {
            id: modeRow
            objectName: "modeRow"
            Layout.fillWidth: true
            visible: root.deviceKnown && root.caps.modes.length > 0
            choices: root.toChoices(root.caps.modes, root.modeLabel)
            current: root.mode
            onChosen: function(value) { root.chooseMode(value) }
        }

        ClimateSetpoint {
            id: setpoint
            objectName: "setpoint"
            Layout.fillWidth: true
            visible: root.deviceKnown && root.setpointKind !== "none"
            bounds: root.caps.bounds
            ranged: root.setpointKind === "range"
            active: root.controlsActive
            value: root.shown("target_temp")
            fallbackValue: root.caps.bounds
                           ? Logic.fallbackTarget(root.devState ? root.devState.current_temp : undefined,
                                                  root.caps.bounds)
                           : 0
            low: root.shown("target_temp_low")
            high: root.shown("target_temp_high")

            onValueEdited: function(v) {
                root.queue({ target_temp: v })
            }
            onRangeEdited: function(range) {
                root.queue(range)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            objectName: "fanSection"
            visible: root.deviceKnown && root.caps.fanModes.length > 0
            spacing: Units.dp(6)

            Text {
                Layout.preferredWidth: Units.dp(56)
                Layout.alignment: Qt.AlignTop
                topPadding: Units.dp(6)
                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                font { family: calaosFont.fontFamilyThin; pointSize: 11 }
                text: qsTr("Fan")
            }

            ClimateChoiceRow {
                id: fanRow
                objectName: "fanRow"
                Layout.fillWidth: true
                active: root.controlsActive
                choices: root.toChoices(root.caps.fanModes, root.optionLabel)
                current: String(root.shown("fan_mode"))
                onChosen: function(value) { root.sendFields({ fan_mode: value }) }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            objectName: "swingSection"
            visible: root.deviceKnown && root.caps.swingModes.length > 0
            spacing: Units.dp(6)

            Text {
                Layout.preferredWidth: Units.dp(56)
                Layout.alignment: Qt.AlignTop
                topPadding: Units.dp(6)
                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                font { family: calaosFont.fontFamilyThin; pointSize: 11 }
                text: qsTr("Swing")
            }

            ClimateChoiceRow {
                id: swingRow
                objectName: "swingRow"
                Layout.fillWidth: true
                active: root.controlsActive
                choices: root.toChoices(root.caps.swingModes, root.optionLabel)
                current: String(root.shown("swing_mode"))
                onChosen: function(value) { root.sendFields({ swing_mode: value }) }
            }
        }
    }
}
