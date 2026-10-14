import QtQuick
import QtTest
import "../../qml/SharedComponents/ClimateLogic.js" as Logic

TestCase {
    name: "ClimateLogic"

    readonly property var bounds: Logic.targetBounds({ target_temp: { min: 16, max: 30, step: 0.5, unit: "°C" } })

    function test_bounds_data() {
        return [
            { tag: "full", caps: { target_temp: { min: 16, max: 30, step: 1, unit: "°F" } },
              min: 16, max: 30, step: 1, unit: "°F", dec: 0 },
            { tag: "empty object takes the spec defaults", caps: { target_temp: {} },
              min: 7, max: 35, step: 0.5, unit: "°C", dec: 1 },
            { tag: "inverted bounds", caps: { target_temp: { min: 30, max: 16, step: 1 } },
              min: 7, max: 35, step: 1, unit: "°C", dec: 0 },
            { tag: "bounds as strings", caps: { target_temp: { min: "16", max: "30" } },
              min: 7, max: 35, step: 0.5, unit: "°C", dec: 1 },
            { tag: "zero step", caps: { target_temp: { min: 10, max: 20, step: 0 } },
              min: 10, max: 20, step: 0.5, unit: "°C", dec: 1 },
            { tag: "quarter step", caps: { target_temp: { min: 10, max: 20, step: 0.25 } },
              min: 10, max: 20, step: 0.25, unit: "°C", dec: 2 },
            { tag: "true", caps: { target_temp: true },
              min: 7, max: 35, step: 0.5, unit: "°C", dec: 1 }
        ]
    }

    function test_bounds(d) {
        var b = Logic.targetBounds(d.caps)
        verify(b !== null)
        compare(b.min, d.min)
        compare(b.max, d.max)
        compare(b.step, d.step)
        compare(b.unit, d.unit)
        compare(b.decimals, d.dec)
    }

    function test_noSetpointCapability_data() {
        return [
            { tag: "absent", caps: {} },
            { tag: "undefined caps", caps: undefined },
            { tag: "null caps", caps: null },
            { tag: "string", caps: { target_temp: "21" } },
            { tag: "false", caps: { target_temp: false } },
            { tag: "list", caps: { target_temp: [16, 30] } }
        ]
    }

    function test_noSetpointCapability(d) {
        compare(Logic.targetBounds(d.caps), null)
        compare(Logic.capabilities(d.caps).bounds, null)
        compare(Logic.capabilities(d.caps).range, false)
    }

    function test_minRange() {
        var caps = Logic.capabilities({ target_temp: { min: 16, max: 30, min_range: 2 }, target_temp_range: true })
        compare(caps.bounds.minRange, 2)
        compare(caps.range, true)
        // a gap wider than the span is capped to the span
        compare(Logic.targetBounds({ target_temp: { min: 16, max: 18, min_range: 5 } }).minRange, 2)
        compare(Logic.targetBounds({ target_temp: { min: 16, max: 30, min_range: -1 } }).minRange, 0)
    }

    function test_rangeIsStrictlyTrue() {
        compare(Logic.capabilities({ target_temp: {}, target_temp_range: "true" }).range, false)
        compare(Logic.capabilities({ target_temp: {} }).range, false)
    }

    function test_rangeOnlyDeviceHasBoundsAndNoSingleLink() {
        // low/high links only: no target_temp capability, spec defaults apply
        var caps = { modes: ["off", "heat"], target_temp_range: true }
        var c = Logic.capabilities(caps, { mode: "m", target_temp_low: "l", target_temp_high: "h" })
        compare(c.range, true)
        compare(c.rangeOnly, true)
        compare(c.bounds.min, 7)
        compare(c.bounds.max, 35)
    }

    function test_rangeOnlyNeedsTheLinkTable() {
        var caps = { target_temp: {}, target_temp_range: true }
        compare(Logic.capabilities(caps, { target_temp: "t", target_temp_low: "l" }).rangeOnly, false)
        compare(Logic.capabilities(caps, {}).rangeOnly, true)
        // no table at all: nothing says the single setpoint is missing
        compare(Logic.capabilities(caps, undefined).rangeOnly, false)
        compare(Logic.capabilities({ target_temp: {} }, {}).rangeOnly, false)
    }

    function test_listsDropJunk() {
        var caps = Logic.capabilities({
            modes: ["off", "", 3, "heat", "heat", null],
            fan_modes: "auto",
            swing_modes: ["both", {}]
        })
        compare(caps.modes, ["off", "heat"])
        compare(caps.fanModes, [])
        compare(caps.swingModes, ["both"])
    }

    // A list inside a QVariantMap is not a JS Array when it comes from C++
    function test_listsFromIOBase() {
        var io = harness.makeClimate({ id: "x", gui_type: "climate", state: "off", device: {
            capabilities: { modes: ["off", "heat"], fan_modes: ["low"], target_temp: {} } } })
        var caps = Logic.capabilities(io.deviceCapabilities)
        compare(caps.modes, ["off", "heat"])
        compare(caps.fanModes, ["low"])
        harness.reset()
    }

    function test_reportsActionIsStrictlyTrue() {
        compare(Logic.capabilities({ reports_action: true }).reportsAction, true)
        compare(Logic.capabilities({ reports_action: "true" }).reportsAction, false)
        compare(Logic.capabilities({}).reportsAction, false)
    }

    function test_snap_data() {
        return [
            { tag: "on the grid", v: 21.5, want: 21.5 },
            { tag: "rounds up", v: 21.3, want: 21.5 },
            { tag: "rounds down", v: 21.2, want: 21 },
            { tag: "float noise", v: 0.1 + 0.2 + 20, want: 20.5 },
            { tag: "below min", v: 3, want: 16 },
            { tag: "above max", v: 99, want: 30 }
        ]
    }

    function test_snap(d) {
        compare(Logic.snap(d.v, bounds), d.want)
    }

    function test_snapIsAnchoredOnMin() {
        var b = Logic.targetBounds({ target_temp: { min: 16.25, max: 30, step: 0.5 } })
        compare(Logic.snap(20, b), 20.25)
    }

    function test_snapNeverLeavesTheBounds() {
        // max is not on the grid: the clamp wins over the step
        var b = Logic.targetBounds({ target_temp: { min: 16, max: 29.8, step: 0.5 } })
        compare(Logic.snap(29.9, b), 29.8)
    }

    function test_stepValue() {
        compare(Logic.stepValue(21, 1, bounds), 21.5)
        compare(Logic.stepValue(21, -1, bounds), 20.5)
        compare(Logic.stepValue(30, 1, bounds), 30)
        compare(Logic.stepValue(16, -1, bounds), 16)
    }

    function test_moveRange_data() {
        return [
            { tag: "low moves freely", low: 18, high: 24, which: "low", minRange: 2, wantLow: 18, wantHigh: 24 },
            { tag: "low stops min_range below high", low: 23.5, high: 24, which: "low", minRange: 2, wantLow: 22, wantHigh: 24 },
            { tag: "high stops min_range above low", low: 20, high: 20.5, which: "high", minRange: 2, wantLow: 20, wantHigh: 22 },
            { tag: "low past high", low: 28, high: 24, which: "low", minRange: 2, wantLow: 22, wantHigh: 24 },
            { tag: "no gap required", low: 24, high: 24, which: "low", minRange: 0, wantLow: 24, wantHigh: 24 },
            { tag: "high capped by max", low: 29, high: 29.5, which: "high", minRange: 2, wantLow: 29, wantHigh: 30 },
            { tag: "off-grid handle floors, never crosses", low: 23.9, high: 25, which: "low", minRange: 1.2, wantLow: 23.5, wantHigh: 25 }
        ]
    }

    function test_moveRange(d) {
        var b = Logic.targetBounds({ target_temp: { min: 16, max: 30, step: 0.5, min_range: d.minRange } })
        var r = Logic.moveRange(d.low, d.high, d.which, b)
        compare(r.low, d.wantLow)
        compare(r.high, d.wantHigh)
    }

    function test_setpointKind_data() {
        var withRange = Logic.capabilities({ target_temp: {}, target_temp_range: true })
        var single = Logic.capabilities({ target_temp: {} })
        var none = Logic.capabilities({})
        var rangeOnly = Logic.capabilities({ target_temp_range: true }, {})
        return [
            { tag: "heat", mode: "heat", caps: withRange, want: "single" },
            { tag: "cool", mode: "cool", caps: withRange, want: "single" },
            { tag: "auto with range", mode: "auto", caps: withRange, want: "range" },
            { tag: "auto without range", mode: "auto", caps: single, want: "single" },
            { tag: "dry", mode: "dry", caps: withRange, want: "single" },
            { tag: "off keeps the control (greyed)", mode: "off", caps: withRange, want: "single" },
            { tag: "unknown keeps the control (greyed)", mode: "", caps: withRange, want: "single" },
            { tag: "fan_only hides it", mode: "fan_only", caps: withRange, want: "none" },
            { tag: "no capability", mode: "heat", caps: none, want: "none" },
            { tag: "range only, heat", mode: "heat", caps: rangeOnly, want: "range" },
            { tag: "range only, cool", mode: "cool", caps: rangeOnly, want: "range" },
            { tag: "range only, off keeps it (greyed)", mode: "off", caps: rangeOnly, want: "range" },
            { tag: "range only, unknown mode", mode: "", caps: rangeOnly, want: "range" },
            { tag: "range only, fan_only hides it", mode: "fan_only", caps: rangeOnly, want: "none" }
        ]
    }

    function test_setpointKind(d) {
        compare(Logic.setpointKind(d.mode, d.caps), d.want)
    }

    function test_controlsActive() {
        compare(Logic.controlsActive("heat"), true)
        compare(Logic.controlsActive("fan_only"), true)
        compare(Logic.controlsActive("off"), false)
        compare(Logic.controlsActive(""), false)
    }

    function test_currentMode() {
        compare(Logic.currentMode("cool"), "cool")
        compare(Logic.currentMode(""), "")
        compare(Logic.currentMode(undefined), "")
        compare(Logic.currentMode(null), "")
        compare(Logic.currentMode(21), "")
    }

    function test_formatting() {
        compare(Logic.formatSetpoint(21, bounds), "21.0 °C")
        compare(Logic.formatSetpoint(undefined, bounds), "—")
        compare(Logic.formatSetpoint("21", bounds), "—")
        compare(Logic.formatRange(19, 24.5, bounds), "19.0 – 24.5 °C")
        compare(Logic.formatRange(19, undefined, bounds), "—")
        compare(Logic.formatReading(23.4, "°C"), "23.4 °C")
        compare(Logic.formatReading(23, "°C"), "23 °C")
        compare(Logic.formatReading(23.449, undefined), "23.4 °C")
        compare(Logic.formatReading(undefined, "°C"), "—")
        compare(Logic.formatReading(NaN, "°C"), "—")
        compare(Logic.formatReading(null, "°C"), "—")
    }

    function test_modeCommandOnlyForOfferedModes() {
        var caps = Logic.capabilities({ modes: ["off", "heat"] })
        compare(Logic.modeCommand("heat", caps), "heat")
        compare(Logic.modeCommand("cool", caps), "")
    }

    function test_partialCommand() {
        compare(Logic.partialCommand({ target_temp: 21.5 }), "{\"target_temp\":21.5}")
        compare(JSON.parse(Logic.partialCommand({ target_temp_low: 19, target_temp_high: 24 })).target_temp_high, 24)
    }

    function test_pickPrefersTheFirstSourceHoldingTheKey() {
        compare(Logic.pick("a", [{ a: 1 }, { a: 2 }]), 1)
        compare(Logic.pick("a", [{}, { a: 2 }]), 2)
        compare(Logic.pick("a", [{}, undefined, null, "x"]), undefined)
        // 0 is a value, not an absence
        compare(Logic.pick("a", [{ a: 0 }, { a: 2 }]), 0)
    }

    function test_confirmedDropsOnlyMatchingFields() {
        var left = Logic.confirmed({ target_temp: 22, fan_mode: "high" }, { target_temp: 22, fan_mode: "low" })
        compare(left, { fan_mode: "high" })
        compare(Logic.confirmed({ target_temp: 22 }, undefined), { target_temp: 22 })
    }

    function test_mergedDoesNotMutate() {
        var a = { x: 1 }
        var m = Logic.merged(a, { y: 2, x: 3 })
        compare(a, { x: 1 })
        compare(m, { x: 3, y: 2 })
    }

    function test_fallbackTarget() {
        compare(Logic.fallbackTarget(23.2, bounds), 23)
        compare(Logic.fallbackTarget(undefined, bounds), 23)
    }

    // --- list item ------------------------------------------------------------

    readonly property var reporting: Logic.capabilities({ reports_action: true })

    function status(mode, state, secs, caps) {
        return Logic.listStatus(true, mode, state, caps || reporting, secs || 0)
    }

    function test_listStatus_data() {
        return [
            { tag: "heating", mode: "heat", state: { action: "heating" },
              kind: "action", tone: "heat", action: "heating", acting: true },
            { tag: "cooling", mode: "cool", state: { action: "cooling" },
              kind: "action", tone: "cool", action: "cooling", acting: true },
            { tag: "drying", mode: "dry", state: { action: "drying" },
              kind: "action", tone: "cool", action: "drying", acting: true },
            { tag: "fan", mode: "fan_only", state: { action: "fan" },
              kind: "action", tone: "white", action: "fan", acting: true },
            { tag: "idle", mode: "auto", state: { action: "idle" },
              kind: "idle", tone: "muted", action: "idle", acting: false },
            { tag: "action not reported by the device", mode: "heat", state: { action: "heating" }, caps: {},
              kind: "none", tone: "muted", action: "", acting: false },
            { tag: "lockout", mode: "heat", state: { action: "idle", lockout: { output: "heat", remaining: 140 } }, secs: 140,
              kind: "lockout", tone: "wait", action: "idle", acting: false, minutes: 2 },
            { tag: "lockout beats the action", mode: "heat", state: { action: "heating" }, secs: 30,
              kind: "lockout", tone: "wait", action: "heating", acting: false, minutes: 1 },
            { tag: "offline", mode: "heat", state: { action: "heating", connected: false },
              kind: "offline", tone: "red", action: "heating", acting: false, modeMuted: true },
            { tag: "offline beats a lockout", mode: "heat", state: { connected: false }, secs: 90,
              kind: "offline", tone: "red", action: "", acting: false, modeMuted: true },
            { tag: "connected true is not offline", mode: "heat", state: { connected: true, action: "heating" },
              kind: "action", tone: "heat", action: "heating", acting: true },
            { tag: "connected as a string is not offline", mode: "heat", state: { connected: "false" },
              kind: "none", tone: "muted", action: "", acting: false },
            { tag: "fault sensor_stale", mode: "heat", state: { fault: "sensor_stale", action: "idle" },
              kind: "fault", tone: "wait", fault: "sensor_stale", action: "idle", acting: false },
            { tag: "fault humidity_high", mode: "cool", state: { fault: "humidity_high" },
              kind: "fault", tone: "wait", fault: "humidity_high", action: "", acting: false },
            { tag: "fault beats a lockout", mode: "cool", state: { fault: "sensor_missing" }, secs: 60,
              kind: "fault", tone: "wait", fault: "sensor_missing", action: "", acting: false },
            { tag: "empty fault is none", mode: "heat", state: { fault: "", action: "heating" },
              kind: "action", tone: "heat", action: "heating", acting: true },
            { tag: "fault that is not a string is ignored", mode: "heat", state: { fault: 3 },
              kind: "none", tone: "muted", action: "", acting: false },
            { tag: "off", mode: "off", state: { action: "idle" },
              kind: "off", tone: "muted", action: "idle", acting: false, modeMuted: true },
            { tag: "off beats a fault", mode: "off", state: { fault: "sensor_stale" },
              kind: "off", tone: "muted", action: "", acting: false, modeMuted: true },
            { tag: "unknown mode", mode: "", state: {},
              kind: "unknown", tone: "muted", action: "", acting: false, modeMuted: true },
            { tag: "unknown mode keeps the action", mode: "", state: { action: "heating" },
              kind: "unknown", tone: "muted", action: "heating", acting: false, modeMuted: true },
            { tag: "unknown mode beats a lockout", mode: "", state: {}, secs: 60,
              kind: "unknown", tone: "muted", action: "", acting: false, modeMuted: true },
            { tag: "no state at all", mode: "heat", state: undefined,
              kind: "none", tone: "muted", action: "", acting: false },
            { tag: "unknown action keeps the muted tone", mode: "heat", state: { action: "defrost" },
              kind: "action", tone: "muted", action: "defrost", acting: false }
        ]
    }

    function test_listStatus(d) {
        var st = status(d.mode, d.state, d.secs, d.caps !== undefined ? Logic.capabilities(d.caps) : undefined)
        compare(st.kind, d.kind)
        compare(st.tone, d.tone)
        compare(st.action, d.action)
        compare(st.acting, d.acting)
        compare(st.fault, d.fault !== undefined ? d.fault : "")
        compare(st.minutes, d.minutes !== undefined ? d.minutes : 0)
        compare(st.modeMuted, d.modeMuted !== undefined ? d.modeMuted : false)
    }

    function test_listStatusWithoutDevice() {
        var st = Logic.listStatus(false, "cool", { action: "cooling", connected: false }, reporting, 0)
        compare(st.kind, "none")
        compare(st.acting, false)
        compare(st.modeMuted, false)
        compare(Logic.listStatus(false, "", undefined, reporting, 0).modeMuted, true)
    }

    function test_lockoutLeft_data() {
        return [
            { tag: "just published", lockout: { remaining: 140 }, elapsed: 0, left: 140 },
            { tag: "counts down", lockout: { remaining: 140 }, elapsed: 20000, left: 120 },
            { tag: "ran out", lockout: { remaining: 140 }, elapsed: 141000, left: 0 },
            { tag: "clock going backwards", lockout: { remaining: 140 }, elapsed: -5000, left: 140 },
            { tag: "absent", lockout: undefined, elapsed: 0, left: 0 },
            { tag: "zero", lockout: { remaining: 0 }, elapsed: 0, left: 0 },
            { tag: "remaining as a string", lockout: { remaining: "140" }, elapsed: 0, left: 0 },
            { tag: "not an object", lockout: 140, elapsed: 0, left: 0 },
            { tag: "no remaining", lockout: { output: "heat" }, elapsed: 0, left: 0 }
        ]
    }

    function test_lockoutLeft(d) {
        compare(Logic.lockoutLeft(d.lockout, d.elapsed), d.left)
    }

    function test_lockoutMinutes_data() {
        return [
            { tag: "rounds to the nearest", secs: 140, min: 2 },
            { tag: "rounds up from 90 s", secs: 90, min: 2 },
            { tag: "rounds down below 90 s", secs: 89, min: 1 },
            { tag: "never 0 while it runs", secs: 5, min: 1 },
            { tag: "long", secs: 1800, min: 30 }
        ]
    }

    function test_lockoutMinutes(d) {
        compare(Logic.lockoutMinutes(d.secs), d.min)
    }

    function test_formatDegrees_data() {
        return [
            { tag: "integer", v: 21, point: ".", text: "21°" },
            { tag: "half", v: 22.5, point: ".", text: "22.5°" },
            { tag: "locale separator", v: 22.5, point: ",", text: "22,5°" },
            { tag: "one decimal at most", v: 21.04, point: ".", text: "21°" },
            { tag: "rounds", v: 18.66, point: ".", text: "18.7°" },
            { tag: "negative", v: -3.5, point: ".", text: "-3.5°" },
            { tag: "absent", v: undefined, point: ".", text: "--" },
            { tag: "string", v: "21", point: ".", text: "--" },
            { tag: "NaN", v: NaN, point: ".", text: "--" }
        ]
    }

    function test_formatDegrees(d) {
        compare(Logic.formatDegrees(d.v, d.point), d.text)
    }

    function test_formatRoomIsEmptyWhenAbsent() {
        compare(Logic.formatRoom(undefined, "."), "")
        compare(Logic.formatRoom(25.4, ","), "25,4°")
    }

    function test_formatListSetpoint_data() {
        return [
            { tag: "single", kind: "single", values: { target: 22 }, text: "22°" },
            { tag: "single with a decimal", kind: "single", values: { target: 20.5 }, text: "20.5°" },
            { tag: "single unknown", kind: "single", values: {}, text: "--" },
            { tag: "range", kind: "range", values: { low: 21, high: 23 }, text: "21 – 23°" },
            { tag: "range with decimals", kind: "range", values: { low: 19.5, high: 24.5 }, text: "19.5 – 24.5°" },
            { tag: "range ignores target", kind: "range", values: { target: 22, low: 21, high: 23 }, text: "21 – 23°" },
            { tag: "range missing a bound", kind: "range", values: { low: 21 }, text: "--" },
            { tag: "range missing both", kind: "range", values: { target: 22 }, text: "--" },
            { tag: "none", kind: "none", values: { target: 22 }, text: "--" }
        ]
    }

    function test_formatListSetpoint(d) {
        compare(Logic.formatListSetpoint(d.kind, d.values, "."), d.text)
    }

    function test_setpointMuted() {
        compare(Logic.setpointMuted({ kind: "offline" }, "heat"), true)
        compare(Logic.setpointMuted({ kind: "off" }, "off"), true)
        compare(Logic.setpointMuted({ kind: "unknown" }, ""), true)
        compare(Logic.setpointMuted({ kind: "action" }, "heat"), false)
        compare(Logic.setpointMuted({ kind: "lockout" }, "heat"), false)
    }

    function test_dialAngle() {
        compare(Logic.dialAngle(16, bounds), -150)
        compare(Logic.dialAngle(30, bounds), 150)
        compare(Logic.dialAngle(23, bounds), 0)
        compare(Logic.dialAngle(5, bounds), -150)
        compare(Logic.dialAngle(99, bounds), 150)
        compare(Logic.dialAngle(undefined, bounds), -150)
    }

    function test_dialModel() {
        var single = Logic.dialModel("single", bounds, { target: 23 }, 16)
        compare(single.fill, [-150, 0])
        compare(single.knobs, [0])
        compare(single.range, null)
        compare(single.tick, -150)

        var range = Logic.dialModel("range", bounds, { low: 16, high: 30, target: 20 }, undefined)
        compare(range.range, [-150, 150])
        compare(range.knobs, [-150, 150])
        compare(range.fill, null)
        compare(range.tick, null)

        var none = Logic.dialModel("single", bounds, {}, undefined)
        compare(none.fill, null)
        compare(none.knobs, [])
        compare(Logic.dialModel("single", null, { target: 20 }, 20).knobs, [])
        compare(Logic.dialModel("none", bounds, { target: 20 }, undefined).fill, null)
        compare(Logic.dialModel("range", bounds, { low: 20 }, undefined).range, null)
    }

}
