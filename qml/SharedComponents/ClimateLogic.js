.pragma library

//Pure helpers behind IOClimate. The `device` object comes from the server and
//is only partly trusted: every reader below accepts a missing or malformed
//field and answers "absent" instead of throwing or inventing a value.

//Spec 6.3: bounds of a setpoint whose linked IO does not define its own.
var DEFAULT_MIN = 7
var DEFAULT_MAX = 35
var DEFAULT_STEP = 0.5
var DEFAULT_UNIT = "°C"

var ABSENT = "—"

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
        reportsAction: caps.reports_action === true
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

function formatSetpoint(v, b) {
    return isNum(v) ? v.toFixed(b.decimals) + " " + b.unit : ABSENT
}

function formatRange(low, high, b) {
    if (!isNum(low) || !isNum(high))
        return ABSENT
    return low.toFixed(b.decimals) + " – " + high.toFixed(b.decimals)
            + " " + b.unit
}

//A reading, not a setpoint: one decimal at most, whatever the step is.
function formatReading(v, unit) {
    if (!isNum(v))
        return ABSENT
    return (Math.round(v * 10) / 10) + " " + (unit || DEFAULT_UNIT)
}

//Plain mode string, or "" when the mode is not one the device offers.
function modeCommand(mode, caps) {
    return caps.modes.indexOf(mode) >= 0 ? mode : ""
}

//Partial update (spec 5.2): only the fields present are applied.
function partialCommand(fields) {
    return JSON.stringify(fields)
}

function hasFields(obj) {
    return Object.keys(obj).length > 0
}

function merged(a, b) {
    var out = {}, k
    for (k in a)
        out[k] = a[k]
    for (k in b)
        out[k] = b[k]
    return out
}

//First of the sources that holds the key.
function pick(key, sources) {
    for (var i = 0; i < sources.length; i++) {
        if (isObj(sources[i]) && sources[i][key] !== undefined)
            return sources[i][key]
    }
    return undefined
}

//Drops from `echoed` the fields the server has now reported with that value.
function confirmed(echoed, state) {
    var out = {}
    for (var k in echoed) {
        if (!isObj(state) || state[k] !== echoed[k])
            out[k] = echoed[k]
    }
    return out
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

//List item: 22.5 -> "22,5°", 21 -> "21°". `point` is the locale decimal separator.
function formatDegrees(v, point) {
    return isNum(v) ? plainNumber(v, point) + "°" : NO_VALUE
}

function plainNumber(v, point) {
    return String(Math.round(v * 10) / 10).replace(".", point || ".")
}

//The reading of the room: "" when the device does not report it.
function formatRoom(v, point) {
    return isNum(v) ? formatDegrees(v, point) : ""
}

//"22°" for a single setpoint, "21 – 23°" for a range, "--" when it is not
//known or the mode has none.
function formatListSetpoint(kind, values, point) {
    if (kind === "range" && isNum(values.low) && isNum(values.high))
        return plainNumber(values.low, point) + " – " + formatDegrees(values.high, point)
    if (kind === "single" && isNum(values.target))
        return formatDegrees(values.target, point)
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
        out.acting = ACTING.indexOf(action) >= 0
        out.tone = ACTION_TONE[action] || "muted"
    }
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
