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

    function test_rangeNeedsASetpoint() {
        compare(Logic.capabilities({ target_temp_range: true }).range, false)
        compare(Logic.capabilities({ target_temp: {}, target_temp_range: "true" }).range, false)
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
        return [
            { tag: "heat", mode: "heat", caps: withRange, want: "single" },
            { tag: "cool", mode: "cool", caps: withRange, want: "single" },
            { tag: "auto with range", mode: "auto", caps: withRange, want: "range" },
            { tag: "auto without range", mode: "auto", caps: single, want: "single" },
            { tag: "dry", mode: "dry", caps: withRange, want: "single" },
            { tag: "off keeps the control (greyed)", mode: "off", caps: withRange, want: "single" },
            { tag: "unknown keeps the control (greyed)", mode: "", caps: withRange, want: "single" },
            { tag: "fan_only hides it", mode: "fan_only", caps: withRange, want: "none" },
            { tag: "no capability", mode: "heat", caps: none, want: "none" }
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

    function test_actionTone() {
        compare(Logic.actionTone("heating"), "warm")
        compare(Logic.actionTone("cooling"), "cold")
        compare(Logic.actionTone("drying"), "dry")
        compare(Logic.actionTone("fan"), "fan")
        compare(Logic.actionTone("idle"), "idle")
        compare(Logic.actionTone("off"), "idle")
        compare(Logic.actionTone("whatever"), "idle")
    }
}
