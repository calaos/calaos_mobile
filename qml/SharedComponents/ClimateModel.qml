pragma ComponentBehavior: Bound

import QtQuick
import "."
import "ClimateLogic.js" as Logic
import "ClimateDrafts.js" as Drafts

//What the climate views know and do for one device (spec 5.3, 6): the
//capabilities, the state with the user's unconfirmed changes laid over it, the
//status to tell, and the commands. The list item and the settings views each
//have one; only the settings views send.
Item {
    id: root

    //The IOBase of the device
    property variant modelData

    visible: false
    width: 0
    height: 0

    readonly property bool deviceKnown: modelData ? modelData.hasDevice : false
    readonly property var caps: Logic.capabilities(modelData ? modelData.deviceCapabilities : undefined,
                                                    modelData && modelData.device ? modelData.device.ios : undefined)
    readonly property var devState: modelData ? modelData.deviceState : undefined
    readonly property string mode: Logic.currentMode(modelData ? modelData.stateString : undefined)
    readonly property bool controlsActive: Logic.controlsActive(mode)
    readonly property string name: modelData ? modelData.ioName : ""
    readonly property string setpointKind: Logic.setpointKind(mode, caps)
    readonly property int decimals: caps.bounds ? caps.bounds.decimals : 1
    readonly property string point: Qt.locale().decimalPoint

    //Status of the device, see Logic.listStatus()
    readonly property var status: Logic.listStatus(deviceKnown, mode, devState, caps, lockoutSeconds)

    //Commands sent and not answered yet, and what is still being typed
    property var drafts: Drafts.empty()
    property var lastState: undefined
    readonly property int inFlight: Drafts.inFlight(drafts)

    readonly property var values: ({ target: shown("target_temp"), low: shown("target_temp_low"),
                                     high: shown("target_temp_high") })
    readonly property real currentTemp: devState && Logic.isNum(devState.current_temp) ? devState.current_temp : NaN

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
    readonly property var knobColors: setpointKind === "range" ? rangeKnobs : [dialKnob]
    readonly property color rangeColor: status.acting ? toneColor(status.tone) : idleArc
    readonly property var dialModel: Logic.dialModel(setpointKind, caps.bounds, values,
                                                     devState ? devState.current_temp : undefined)

    //The server publishes the seconds left once; the countdown runs here
    property var clock: Date.now
    property real now: 0
    property real lockoutStamp: 0
    readonly property real lockoutSeconds: Logic.lockoutLeft(devState ? devState.lockout : undefined,
                                                              now - lockoutStamp)
    readonly property alias lockoutTicking: lockoutTimer.running

    function restampLockout() {
        now = clock()
        lockoutStamp = now
    }

    function reset() {
        flushTimer.stop()
        setDrafts(Drafts.empty())
        lastState = devState
        restampLockout()
    }

    onDevStateChanged: restampLockout()
    onModelDataChanged: reset()
    Component.onCompleted: reset()

    function shown(key) {
        return Drafts.shown(drafts, key, devState)
    }

    function setDrafts(d) {
        drafts = d
        if (Drafts.inFlight(d) > 0)
            expiryTimer.start()
        else
            expiryTimer.stop()
    }

    //Edits wait here so that a burst of taps makes one command
    function queue(fields) {
        drafts = Drafts.queue(drafts, fields)
        flushTimer.restart()
    }

    function flush() {
        flushTimer.stop()
        if (!modelData || !Drafts.hasPending(drafts))
            return
        var r = Drafts.send(drafts, devState)
        drafts = r.drafts
        if (Drafts.inFlight(r.drafts) > 0)
            expiryTimer.restart()
        modelData.sendStringValue(Logic.partialCommand(r.fields))
    }

    //Fan and swing are sent at once, as a partial update
    function commit(fields) {
        queue(fields)
        flush()
    }

    function chooseMode(value) {
        var cmd = Logic.modeCommand(value, caps)
        if (cmd !== "" && cmd !== mode)
            modelData.sendStringValue(cmd)
    }


    //Which setpoint a -/+ pair moves: "target", or "low" / "high" in a range
    function stepKey(which) {
        return which === "target" ? "target_temp" : which === "low" ? "target_temp_low" : "target_temp_high"
    }

    function fallback() {
        return caps.bounds ? Logic.fallbackTarget(devState ? devState.current_temp : undefined, caps.bounds) : 0
    }

    function canStep(which, dir) {
        if (!deviceKnown || !controlsActive || setpointKind === "none" || status.kind === "offline")
            return false
        return Logic.canStep(which, dir, values, caps.bounds)
    }

    function step(which, dir) {
        if (!canStep(which, dir))
            return
        var v = Logic.stepSetpoint(which, dir, values, caps.bounds, fallback())
        var f = {}
        f[stepKey(which)] = v
        queue(f)
    }

    readonly property bool rangeAtStop: setpointKind === "range" && Logic.rangeAtStop(values, caps.bounds)

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

    function statusLabel(st, detailed) {
        switch (st.kind) {
        case "offline": return qsTr("Offline")
        //The pill already says the mode is unknown; the action is more to the point
        case "unknown": return st.action !== "" ? actionLabel(st.action) : ""
        case "off": return qsTr("Stopped")
        case "fault": return faultLabel(st.fault)
        case "lockout": return detailed ? lockoutLabel(lockoutSeconds)
                                        : qsTr("Starts in %n min", "", st.minutes)
        case "action":
        case "idle": return actionLabel(st.action)
        default: return ""
        }
    }

    //The settings views count the seconds
    function lockoutLabel(seconds) {
        var d = Logic.splitDuration(seconds)
        return d.minutes > 0 ? qsTr("Starts in %1 min %2 s").arg(d.minutes).arg(d.seconds)
                             : qsTr("Starts in %1 s").arg(d.seconds)
    }

    function toChoices(values, labeler) {
        return values.map(function(v) { return { value: v, label: labeler(v) } })
    }

    Timer {
        id: flushTimer
        interval: 400
        onTriggered: root.flush()
    }

    //Nothing heard for a while: the state the server shows is the truth
    Timer {
        id: expiryTimer
        interval: 2500
        onTriggered: root.setDrafts(Drafts.expired(root.drafts))
    }

    Timer {
        id: lockoutTimer
        interval: 1000
        repeat: true
        running: root.status.kind === "lockout"
        onTriggered: root.now = root.clock()
    }

    Connections {
        target: root.modelData ? root.modelData : null

        function onDeviceChange() {
            var state = root.modelData.deviceState
            root.setDrafts(Drafts.received(root.drafts, root.lastState, state))
            root.lastState = state
        }

        //The server will not apply what the oldest command asked
        function onCommandRefused() {
            root.setDrafts(Drafts.refused(root.drafts))
        }
    }

    //A view closed or scrolled away must not drop an edit that is still
    //waiting for the end of the burst
    Component.onDestruction: {
        if (modelData)
            flush()
    }
}
