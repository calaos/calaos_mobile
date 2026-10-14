.pragma library

//Pure helpers behind IOClimate. The `device` object comes from the server and
//is only partly trusted: every reader below accepts a missing or malformed
//field and answers "absent" instead of throwing or inventing a value.

//Spec 6.3: bounds of a setpoint whose linked IO does not define its own.
var DEFAULT_MIN = 7
var DEFAULT_MAX = 35
var DEFAULT_STEP = 0.5
var DEFAULT_UNIT = "°C"

function isNum(v) {
    return typeof v === "number" && isFinite(v)
}

function isObj(v) {
    return v !== null && typeof v === "object" && !Array.isArray(v)
}

//A list nested in a QVariantMap reaches JS as a sequence wrapper, for which
//Array.isArray() is false: only the length and the indexes can be trusted.
function isList(v) {
    return v !== null && typeof v === "object" && typeof v.length === "number"
}

function stringList(v) {
    var out = []
    if (!isList(v))
        return out
    for (var i = 0; i < v.length; i++) {
        if (typeof v[i] === "string" && v[i] !== "" && out.indexOf(v[i]) < 0)
            out.push(v[i])
    }
    return out
}

function decimals(step) {
    for (var d = 0; d < 4; d++) {
        var f = Math.pow(10, d)
        if (Math.abs(Math.round(step * f) - step * f) < 1e-9)
            return d
    }
    return 4
}

function round6(v) {
    return Math.round(v * 1e6) / 1e6
}

//Bounds of the target_temp capability, or null when the device has no
//setpoint at all. min_range is only meaningful with target_temp_range.
function targetBounds(caps) {
    var t = caps ? caps.target_temp : undefined
    if (!isObj(t)) {
        //A device with only low/high links has no target_temp capability of
        //its own but still has bounds to apply to its range
        if (t !== true && !(caps && caps.target_temp_range === true))
            return null
        t = {}
    }

    var min = t.min, max = t.max
    if (!isNum(min) || !isNum(max) || max <= min) {
        min = DEFAULT_MIN
        max = DEFAULT_MAX
    }
    var step = isNum(t.step) && t.step > 0 ? t.step : DEFAULT_STEP
    var unit = typeof t.unit === "string" && t.unit !== "" ? t.unit : DEFAULT_UNIT
    var minRange = isNum(t.min_range) && t.min_range > 0 ? t.min_range : 0
    minRange = Math.min(minRange, max - min)

    return { min: min, max: max, step: step, unit: unit, minRange: minRange,
             decimals: decimals(step) }
}

//Everything the widget reads from the capabilities, normalized once. `ios` is
//the device's link table: a range without a target_temp link is a device that
//only takes low/high, and a lone target_temp command would always be refused.
function capabilities(caps, ios) {
    if (!isObj(caps))
        caps = {}
    var bounds = targetBounds(caps)
    var range = bounds !== null && caps.target_temp_range === true
    return {
        modes: stringList(caps.modes),
        bounds: bounds,
        range: range,
        rangeOnly: range && isObj(ios) && !ios.target_temp,
        fanModes: stringList(caps.fan_modes),
        swingModes: stringList(caps.swing_modes),
        reportsAction: caps.reports_action === true,
        humidity: caps.humidity === true
    }
}

//The scalar `state` of the item is the mode; "" means it is not known (yet).
function currentMode(scalarState) {
    return typeof scalarState === "string" ? scalarState : ""
}

//Off and an unknown mode both grey the setpoint and the selectors: in neither
//does a change have a defined effect.
function controlsActive(mode) {
    return mode !== "" && mode !== "off"
}

//"none", "single" or "range". fan_only has no temperature to aim for; a range
//is shown in auto, and in every mode when it is all the device has.
function setpointKind(mode, caps) {
    if (caps.bounds === null || mode === "fan_only")
        return "none"
    if (caps.rangeOnly)
        return "range"
    if (mode === "auto" && caps.range)
        return "range"
    return "single"
}

//Rounds to the step grid anchored on min, then clamps. dir: "round" (default),
//"floor" or "ceil", for callers that must not cross a limit when snapping.
function snap(v, b, dir) {
    var n = (v - b.min) / b.step
    n = dir === "floor" ? Math.floor(n + 1e-9)
      : dir === "ceil" ? Math.ceil(n - 1e-9)
      : Math.round(n)
    var r = round6(b.min + n * b.step)
    return Math.min(b.max, Math.max(b.min, r))
}

function stepValue(v, dir, b) {
    return snap(v + dir * b.step, b)
}

//Applies the user's move of one handle and keeps the other one still, with at
//least min_range between them. Returns { low, high }.
function moveRange(low, high, which, b) {
    if (which === "low")
        low = snap(Math.min(low, high - b.minRange), b, "floor")
    else
        high = snap(Math.max(high, low + b.minRange), b, "ceil")
    return { low: low, high: high }
}

//Starting point of a setpoint the server never reported.
function fallbackTarget(currentTemp, b) {
    if (isNum(currentTemp))
        return snap(currentTemp, b)
    return snap((b.min + b.max) / 2, b)
}

//`point` is the decimal separator of the locale, "." when left out.
function withPoint(text, point) {
    return point ? text.replace(".", point) : text
}

//Plain mode string, or "" when the mode is not one the device offers.
function modeCommand(mode, caps) {
    return caps.modes.indexOf(mode) >= 0 ? mode : ""
}

//Partial update (spec 5.2): only the fields present are applied.
function partialCommand(fields) {
    return JSON.stringify(fields)
}

//Dial: 300 degrees, open at the bottom, angles counted clockwise from 12 o'clock.
var ARC_START = -150
var ARC_SPAN = 300

function dialAngle(t, b) {
    if (!isNum(t) || b === null || b === undefined)
        return ARC_START
    var f = Math.min(1, Math.max(0, (t - b.min) / (b.max - b.min)))
    return ARC_START + ARC_SPAN * f
}

var NO_VALUE = "--"

//List item: 22.5 -> "22,5°", 21 -> "21°". At most `decimals` digits (one when left
//out), trailing zeros dropped; `point` is the decimal separator of the locale.
function formatDegrees(v, point, decimals) {
    return isNum(v) ? plainNumber(v, point, decimals) + "°" : NO_VALUE
}

function plainNumber(v, point, decimals) {
    var d = decimals === undefined ? 1 : decimals
    var text = v.toFixed(d)
    if (d > 0)
        text = text.replace(/0+$/, "").replace(/\.$/, "")
    return withPoint(text, point)
}

//The reading of the room: "" when the device does not report it.
function formatRoom(v, point) {
    return isNum(v) ? formatDegrees(v, point) : ""
}

//"22°" for a single setpoint, "21 – 23°" for a range, "--" when it is not
//known or the mode has none. `decimals` follows the step of the setpoint.
function formatListSetpoint(kind, values, point, decimals) {
    if (kind === "range" && isNum(values.low) && isNum(values.high))
        return plainNumber(values.low, point, decimals) + " – " + formatDegrees(values.high, point, decimals)
    if (kind === "single" && isNum(values.target))
        return formatDegrees(values.target, point, decimals)
    return NO_VALUE
}

//Seconds left on a lock-out from the value the server published and the time
//elapsed since it did; 0 when there is none (or it ran out).
function lockoutLeft(lockout, elapsedMs) {
    if (!isObj(lockout) || !isNum(lockout.remaining) || lockout.remaining <= 0)
        return 0
    return Math.max(0, lockout.remaining - Math.max(0, elapsedMs) / 1000)
}

//Whole minutes shown for a lock-out, never "0 min" while one is running.
function lockoutMinutes(seconds) {
    return Math.max(1, Math.round(seconds / 60))
}

var ACTING = ["heating", "cooling", "drying", "fan"]
var ACTION_TONE = { heating: "heat", cooling: "cool", drying: "cool", fan: "white" }

//What the list item says about the device, one status at a time. Priority:
//offline, mode unknown, off, safety fault, lock-out, action. `action` is only
//read when the device reports one.
//kind: none | offline | unknown | off | fault | lockout | action | idle
//tone: heat | cool | wait | red | white | muted
function listStatus(deviceKnown, mode, state, caps, lockoutSeconds) {
    var out = { kind: "none", tone: "muted", action: "", fault: "", minutes: 0,
                acting: false, modeMuted: mode === "" || mode === "off" }
    if (!deviceKnown)
        return out

    var s = isObj(state) ? state : {}
    var action = typeof s.action === "string" && caps && caps.reportsAction ? s.action : ""
    out.action = action
    out.acting = ACTING.indexOf(action) >= 0

    if (s.connected === false) {
        out.kind = "offline"
        out.tone = "red"
        out.modeMuted = true
    } else if (mode === "") {
        out.kind = "unknown"
    } else if (mode === "off") {
        out.kind = "off"
    } else if (typeof s.fault === "string" && s.fault !== "") {
        out.kind = "fault"
        out.tone = "wait"
        out.fault = s.fault
    } else if (lockoutSeconds > 0) {
        out.kind = "lockout"
        out.tone = "wait"
        out.minutes = lockoutMinutes(lockoutSeconds)
    } else if (action !== "") {
        out.kind = action === "idle" || action === "off" ? "idle" : "action"
        out.tone = ACTION_TONE[action] || "muted"
    }
    //A device that is offline, off or in an unknown mode is not doing what it last reported
    if (out.kind === "offline" || out.kind === "unknown" || out.kind === "off")
        out.acting = false
    return out
}

//Whether the list item shows the setpoint dimmed: nothing can be set.
function setpointMuted(status, mode) {
    return status.kind === "offline" || mode === "" || mode === "off"
}

//What the mini dial draws. Values are temperatures; the caller colours them.
//{ fill: [from, to] | null, range: [from, to] | null, knobs: [angle...], tick: angle | null }
function dialModel(kind, bounds, values, current) {
    var out = { fill: null, range: null, knobs: [], tick: null }
    if (bounds === null || bounds === undefined)
        return out
    if (kind === "range" && isNum(values.low) && isNum(values.high)) {
        var lo = dialAngle(values.low, bounds), hi = dialAngle(values.high, bounds)
        out.range = [lo, hi]
        out.knobs = [lo, hi]
    } else if (kind === "single" && isNum(values.target)) {
        var a = dialAngle(values.target, bounds)
        out.fill = [ARC_START, a]
        out.knobs = [a]
    }
    if (isNum(current))
        out.tick = dialAngle(current, bounds)
    return out
}

//One tap on a - / + of the setpoint line. `which`: "target", "low" or "high".
//Returns the new value, or the current one when the limit is reached; a
//value that is not known yet starts from `fallback`.
function stepSetpoint(which, dir, values, b, fallback) {
    if (which === "target")
        return stepValue(isNum(values.target) ? values.target : fallback, dir, b)
    var low = isNum(values.low) ? values.low : fallback
    var high = isNum(values.high) ? values.high : fallback
    var r = which === "low" ? moveRange(low + dir * b.step, high, "low", b)
                            : moveRange(low, high + dir * b.step, "high", b)
    return which === "low" ? r.low : r.high
}

//Whether the button does anything: the inner ones are refused at min_range.
function canStep(which, dir, values, b) {
    if (b === null || b === undefined)
        return false
    var current = values[which]
    if (!isNum(current))
        return true
    //A range already tighter than min_range would be pushed back the other way
    return (stepSetpoint(which, dir, values, b, current) - current) * dir > 0
}

//The two ends are as close as min_range allows.
function rangeAtStop(values, b) {
    return b !== null && b !== undefined && b.minRange > 0 && isNum(values.low) && isNum(values.high)
            && values.high - values.low <= b.minRange + 1e-6
}

//Seconds as { minutes, seconds } for the countdown of a lock-out.
function splitDuration(seconds) {
    //Rounded up: while the lock-out runs the count never says "0 s"
    var s = Math.max(0, Math.ceil(seconds))
    return { minutes: Math.floor(s / 60), seconds: s % 60 }
}

//Whether the "more settings" page has anything to show.
function hasMore(caps, state) {
    return caps.fanModes.length > 0 || caps.swingModes.length > 0 || humidityShown(caps, state)
}

//Humidity is announced by the capabilities and read from the state, both optional.
function humidityShown(caps, state) {
    return caps.humidity === true && isObj(state) && isNum(state.humidity)
}
