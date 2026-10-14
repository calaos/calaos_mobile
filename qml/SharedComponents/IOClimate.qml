pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."
import "ClimateLogic.js" as Logic

//Climate device (spec 5.3, 6): everything it shows comes from the capabilities
//of the `device` object, so one widget serves every impl. The item's scalar
//state is the mode; "" means the mode is not known.
//Collapsed it is the list item of UI_CLIMATE.md; a tap on it opens the controls
//below it, which the bottom sheet / dialog of the final design will replace.
ItemBase {
    id: root

    property variant modelData

    readonly property bool deviceKnown: modelData ? modelData.hasDevice : false
    readonly property var caps: Logic.capabilities(modelData ? modelData.deviceCapabilities : undefined,
                                                    modelData && modelData.device ? modelData.device.ios : undefined)
    readonly property var devState: modelData ? modelData.deviceState : undefined
    readonly property string mode: Logic.currentMode(modelData ? modelData.stateString : undefined)
    readonly property bool controlsActive: Logic.controlsActive(mode)

    //Setpoint edits wait here so that a drag or a few taps on +/- make one
    //command; what was sent is held in `echoed` until the server reports it,
    //otherwise the control would jump back to the old value in between.
    property var pending: ({})
    property var echoed: ({})

    //Status of the device for the list item, see Logic.listStatus()
    readonly property var status: Logic.listStatus(deviceKnown, mode, devState, caps, lockoutSeconds)
    readonly property string setpointKind: Logic.setpointKind(mode, caps)
    property bool expanded: false
    readonly property alias lockoutTicking: lockoutTimer.running

    readonly property var listValues: ({ target: shown("target_temp"), low: shown("target_temp_low"),
                                         high: shown("target_temp_high") })
    //A dial with nothing to say about the action: grey arc, white knob
    readonly property bool plainDial: status.kind === "off" ||
        (status.kind !== "offline" && status.kind !== "unknown" && status.kind !== "lockout" && !status.acting)
    readonly property color idleArc: "#4a4a4a"
    readonly property color dialFill: status.kind === "offline" ? idleArc
                                      : plainDial ? Theme.mutedColor : toneColor(status.tone)
    readonly property color dialKnob: status.kind === "offline" ? Theme.mutedColor
                                      : plainDial ? Theme.whiteColor : toneColor(status.tone)
    //Auto: yellow and cyan for the two ends, neutral when the device is not running, amber while it waits
    readonly property var rangeKnobs: status.kind === "lockout" ? [Theme.amberColor, Theme.amberColor]
                                      : (status.kind === "offline" || status.kind === "unknown") ? [Theme.mutedColor, Theme.mutedColor]
                                      : status.kind === "off" ? [Theme.whiteColor, Theme.whiteColor]
                                      : [Theme.yellowColor, Theme.blueColor]

    //The server publishes the seconds left once; the countdown runs here
    property var clock: Date.now
    property real now: 0
    property real lockoutStamp: 0
    readonly property real lockoutSeconds: Logic.lockoutLeft(devState ? devState.lockout : undefined,
                                                              now - lockoutStamp)

    height: layout.implicitHeight

    function restampLockout() {
        now = clock()
        lockoutStamp = now
    }

    onDevStateChanged: restampLockout()
    Component.onCompleted: restampLockout()

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
        //echoed first: clearing pending alone would show the old value for a beat
        var fields = pending
        sendFields(fields)
        pending = ({})
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
        case "heat": return Theme.yellowColor
        case "cool": return Theme.blueColor
        case "wait": return Theme.amberColor
        case "red": return Theme.redColor
        case "white": return Theme.whiteColor
        default: return Theme.mutedColor
        }
    }

    function modeIcon(m) {
        switch (m) {
        case "off": return "power"
        case "heat": return "flame"
        case "cool": return "snow"
        case "auto": return "autoArrows"
        case "dry": return "drop"
        case "fan_only": return "fan"
        default: return "help"
        }
    }

    function faultLabel(f) {
        switch (f) {
        case "sensor_stale": return qsTr("Sensor silent")
        case "sensor_missing": return qsTr("Sensor missing")
        case "humidity_high": return qsTr("Humidity too high")
        case "humidity_stale": return qsTr("Humidity sensor silent")
        default: return humanize(f)
        }
    }

    function statusLabel(st) {
        switch (st.kind) {
        case "offline": return qsTr("Offline")
        //The pill already says the mode is unknown; the action is more to the point
        case "unknown": return st.action !== "" ? actionLabel(st.action) : ""
        case "off": return qsTr("Stopped")
        case "fault": return faultLabel(st.fault)
        case "lockout": return qsTr("Starts in %n min", "", st.minutes)
        case "action":
        case "idle": return actionLabel(st.action)
        default: return ""
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

        //The server will not apply what was sent: show the state it holds now
        function onCommandRefused() {
            echoTimer.stop()
            root.echoed = ({})
        }
    }

    //A card scrolled out of the list or closed must not drop an edit that is
    //still waiting for the end of the burst
    Component.onDestruction: {
        if (modelData)
            flush()
    }

    Timer {
        id: lockoutTimer
        interval: 1000
        repeat: true
        running: root.status.kind === "lockout"
        onTriggered: root.now = root.clock()
    }

    ColumnLayout {
        id: layout

        anchors { left: parent.left; right: parent.right; top: parent.top }
        spacing: 0

        ClimateListItem {
            id: head
            Layout.fillWidth: true

            fontFamily: calaosFont.fontFamily
            name: root.modelData ? root.modelData.ioName : ""
            nameActive: root.status.acting
            roomText: Logic.formatRoom(root.devState ? root.devState.current_temp : undefined, Qt.locale().decimalPoint)
            modeVisible: root.deviceKnown ? true : root.mode !== ""
            modeText: root.mode !== "" ? root.modeLabel(root.mode) : qsTr("Mode unknown")
            modeIcon: root.modeIcon(root.mode)
            modeMuted: root.status.modeMuted
            statusText: root.statusLabel(root.status)
            statusColor: root.toneColor(root.status.tone)
            setpointText: Logic.formatListSetpoint(root.setpointKind, root.listValues, Qt.locale().decimalPoint,
                                                   root.caps.bounds ? root.caps.bounds.decimals : undefined)
            setpointMuted: Logic.setpointMuted(root.status, root.mode)
            expanded: root.expanded

            onClicked: root.expanded = !root.expanded
            onPressedChanged: root.__inBorder.state = pressed ? "pressed" : "released"
            onHoveredChanged: root.state = hovered ? "hovered" : "normal"

            dial.model: Logic.dialModel(root.setpointKind, root.caps.bounds, root.listValues,
                                        root.devState ? root.devState.current_temp : undefined)
            dial.fillColor: root.dialFill
            dial.rangeColor: root.status.acting ? root.toneColor(root.status.tone) : root.idleArc
            dial.tickColor: root.status.kind === "offline" ? Theme.mutedColor : "#ffffff"
            dial.knobColors: root.setpointKind === "range" ? root.rangeKnobs : [root.dialKnob]
        }

        //Built on the first tap: a room lists many of these and most stay closed
        Loader {
            id: settings
            objectName: "settings"

            active: root.expanded
            visible: root.expanded
            Layout.fillWidth: true
            Layout.preferredHeight: item ? (item as Item).implicitHeight : 0
            Layout.leftMargin: Units.dp(8)
            Layout.rightMargin: Units.dp(8)
            Layout.bottomMargin: Units.dp(8)

            sourceComponent: ColumnLayout {
                spacing: Units.dp(6)

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
                    range: ({ low: root.shown("target_temp_low"), high: root.shown("target_temp_high") })

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
    }
}
