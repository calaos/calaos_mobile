import QtQuick
import QtTest
import QtQuick.Shapes
import SharedComponents

//The collapsed IOClimate, i.e. the list item of UI_CLIMATE.md, one row of the
//"displayed status" table at a time.
TestCase {
    id: tc
    name: "ClimateListItem"
    when: windowShown
    visible: true
    width: 420
    height: 600

    readonly property string point: Qt.locale().decimalPoint

    property real fakeNow: 1000000
    function fakeClock() { return fakeNow }

    Component {
        id: itemComponent
        IOClimate { width: 400; clock: tc.fakeClock }
    }

    function caps() {
        return {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "low"],
            reports_action: true
        }
    }

    function mkState(over) {
        var st = { mode: "cool", current_temp: 25.4, target_temp: 22, target_temp_low: 20, target_temp_high: 24,
                   action: "cooling" }
        for (var k in over) {
            if (over[k] === undefined)
                delete st[k]
            else
                st[k] = over[k]
        }
        return st
    }

    function device(st, capsOver) {
        var c = caps()
        for (var k in capsOver)
            c[k] = capsOver[k]
        return { family: "climate", impl: "proxy", capabilities: c, state: st,
                 ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } }
    }

    function make(over, capsOver) {
        var st = mkState(over)
        var item = { id: "salon_ac", name: "Clim salon", gui_type: "climate", state: st.mode === undefined ? "" : st.mode,
                     rw: "true", device: device(st, capsOver) }
        var w = createTemporaryObject(itemComponent, tc, { modelData: harness.makeClimate(item) })
        verify(w !== null)
        return w
    }

    function find(item, name) {
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var r = find(item.children[i], name)
            if (r)
                return r
        }
        return null
    }

    function text(w, name) { return find(w, name).text }

    function init() {
        harness.reset()
        fakeNow = 1000000
    }

    // --- status: one row of the table each -----------------------------------------

    function test_actionStatus_data() {
        return [
            { tag: "heating", mode: "heat", action: "heating", text: "Heating", color: Theme.yellowColor },
            { tag: "cooling", mode: "cool", action: "cooling", text: "Cooling", color: Theme.blueColor },
            { tag: "drying", mode: "dry", action: "drying", text: "Drying", color: Theme.blueColor },
            { tag: "fan", mode: "fan_only", action: "fan", text: "Fan", color: Theme.whiteColor },
            { tag: "idle", mode: "auto", action: "idle", text: "Idle", color: Theme.mutedColor }
        ]
    }

    function test_actionStatus(d) {
        var w = make({ mode: d.mode, action: d.action })
        compare(text(w, "statusText"), d.text)
        compare(find(w, "statusText").color, d.color)
    }

    function test_lockoutStatusCountsDownLocally() {
        var w = make({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        compare(text(w, "statusText"), "Starts in 2 min")
        compare(find(w, "statusText").color, Theme.amberColor)

        fakeNow += 60000
        tryCompare(find(w, "statusText"), "text", "Starts in 1 min", 3000)

        // 140 s - 141 s: it ran out, the action reported by the server is back
        fakeNow += 81000
        tryCompare(find(w, "statusText"), "text", "Idle", 3000)
        compare(find(w, "statusText").color, Theme.mutedColor)
    }

    function test_lockoutIsRestartedByANewFrame() {
        var w = make({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        fakeNow += 100000
        harness.pushFrame("salon_ac", undefined,
            device(mkState({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 300 } })))
        // let the clock tick: the countdown must restart from the new value, not from the old stamp
        wait(1300)
        compare(text(w, "statusText"), "Starts in 5 min")
    }

    function test_lockoutGoneWhenTheServerDropsIt() {
        var w = make({ mode: "heat", action: "heating", lockout: { output: "heat", remaining: 140 } })
        compare(text(w, "statusText"), "Starts in 2 min")
        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", action: "heating" })))
        compare(text(w, "statusText"), "Heating")
    }

    function test_offlineStatus() {
        var w = make({ mode: "heat", action: "heating", connected: false })
        compare(text(w, "statusText"), "Offline")
        compare(find(w, "statusText").color, Theme.redColor)
        // the pill goes grey, the name is no longer "acting"
        compare(find(w, "modeText").color, Theme.mutedColor)
        compare(find(w, "itemName").color, Theme.blueColor)
        compare(find(w, "setpointText").color, Theme.mutedColor)
    }

    function test_faultStatus_data() {
        return [
            { tag: "sensor_stale", fault: "sensor_stale", text: "Sensor silent" },
            { tag: "sensor_missing", fault: "sensor_missing", text: "Sensor missing" },
            { tag: "humidity_high", fault: "humidity_high", text: "Humidity too high" },
            { tag: "humidity_stale", fault: "humidity_stale", text: "Humidity sensor silent" },
            { tag: "another reason is humanized", fault: "valve_stuck", text: "Valve stuck" }
        ]
    }

    function test_faultStatus(d) {
        var w = make({ mode: "heat", action: "idle", fault: d.fault })
        compare(text(w, "statusText"), d.text)
        compare(find(w, "statusText").color, Theme.amberColor)
    }

    function test_offStatus() {
        var w = make({ mode: "off", action: "idle" })
        compare(text(w, "statusText"), "Stopped")
        compare(find(w, "statusText").color, Theme.mutedColor)
        compare(text(w, "modeText"), "Off")
        compare(find(w, "modeText").color, Theme.mutedColor)
        compare(find(w, "modeIcon").name, "power")
        compare(find(w, "setpointText").color, Theme.mutedColor)
    }

    function test_unknownModeIsAGreyPill() {
        var w = make({ mode: "", action: undefined })
        compare(text(w, "modeText"), "Mode unknown")
        compare(find(w, "modeText").color, Theme.mutedColor)
        compare(find(w, "modeIcon").name, "help")
        compare(find(w, "modePill").visible, true)
        // said once, in the pill
        compare(text(w, "statusText"), "")
    }

    function test_unknownModeTellsTheReportedAction() {
        var w = make({ mode: "", action: "idle" })
        compare(text(w, "modeText"), "Mode unknown")
        compare(text(w, "statusText"), "Idle")
    }

    function test_absentModeIsUnknownToo() {
        var st = mkState({ mode: undefined, action: undefined })
        var item = { id: "salon_ac", name: "Clim", gui_type: "climate", device: device(st) }
        var w = createTemporaryObject(itemComponent, tc, { modelData: harness.makeClimate(item) })
        compare(text(w, "modeText"), "Mode unknown")
        compare(find(w, "modeText").color, Theme.mutedColor)
    }

    function test_statusPriority() {
        // offline over everything, then the unknown mode, off, a fault, a lockout
        var all = { mode: "heat", action: "heating", connected: false, fault: "sensor_stale",
                    lockout: { output: "heat", remaining: 140 } }
        compare(text(make(all), "statusText"), "Offline")
        all.connected = true
        compare(text(make(all), "statusText"), "Sensor silent")
        delete all.fault
        compare(text(make(all), "statusText"), "Starts in 2 min")
    }

    // --- mode pill ------------------------------------------------------------------

    function test_modePill_data() {
        return [
            { tag: "heat", mode: "heat", label: "Heat", icon: "flame" },
            { tag: "cool", mode: "cool", label: "Cool", icon: "snow" },
            { tag: "auto", mode: "auto", label: "Auto", icon: "autoArrows" },
            { tag: "dry", mode: "dry", label: "Dry", icon: "drop" },
            { tag: "fan_only", mode: "fan_only", label: "Fan only", icon: "fan" }
        ]
    }

    function test_modePill(d) {
        var w = make({ mode: d.mode })
        compare(text(w, "modeText"), d.label)
        compare(find(w, "modeIcon").name, d.icon)
        compare(find(w, "modeText").color, Theme.blueColor)
        compare(find(w, "modeIcon").color, Theme.blueColor)
        var pill = find(w, "modePill")
        compare(pill.border.width, 1)
        fuzzyCompare(pill.color.a, 0.12, 0.01)
        fuzzyCompare(pill.border.color.a, 0.5, 0.01)
    }

    function test_greyPillHasNoCyan() {
        var pill = find(make({ mode: "off" }), "modePill")
        fuzzyCompare(pill.color.r, 1, 0.01)
        fuzzyCompare(pill.color.a, 0.04, 0.01)
        // the muted grey of the text, not the cyan
        fuzzyCompare(pill.border.color.r, 0.525, 0.02)
        fuzzyCompare(pill.border.color.a, 0.45, 0.01)
    }

    // --- setpoint ---------------------------------------------------------------------

    function test_singleSetpoint() {
        var w = make({ mode: "cool", target_temp: 22 })
        compare(text(w, "setpointText"), "22°")
        compare(find(w, "setpointText").color, Theme.yellowColor)
    }

    function test_autoShowsTheRange() {
        var w = make({ mode: "auto", action: "idle", target_temp_low: 21, target_temp_high: 23 })
        compare(text(w, "setpointText"), "21 – 23°")
        compare(find(w, "setpointText").color, Theme.yellowColor)
    }

    function test_autoRangeWithDecimals() {
        var w = make({ mode: "auto", target_temp_low: 19.5, target_temp_high: 24.5 })
        compare(text(w, "setpointText"), "19" + point + "5 – 24" + point + "5°")
    }

    function test_autoRangeNeverReportedIsADash() {
        var w = make({ mode: "auto", target_temp_low: undefined, target_temp_high: undefined })
        compare(text(w, "setpointText"), "--")
    }

    function test_unknownSetpointIsADash() {
        compare(text(make({ mode: "heat", target_temp: undefined }), "setpointText"), "--")
        compare(text(make({ mode: "", target_temp: undefined, action: undefined }), "setpointText"), "--")
    }

    function test_fanOnlyHasNoSetpoint() {
        compare(text(make({ mode: "fan_only", action: "fan" }), "setpointText"), "--")
    }

    function test_rangeOnlyDeviceShowsItsRangeInEveryMode() {
        var c = caps()
        delete c.target_temp
        var st = mkState({ mode: "heat", target_temp_low: 19, target_temp_high: 24 })
        var dev = { family: "climate", impl: "proxy", capabilities: c, state: st,
                    ios: { mode: "m", target_temp_low: "l", target_temp_high: "h" } }
        var w = createTemporaryObject(itemComponent, tc, { modelData: harness.makeClimate(
            { id: "x", name: "X", gui_type: "climate", state: "heat", device: dev }) })
        compare(text(w, "setpointText"), "19 – 24°")
    }

    function test_setpointFollowsTheServer() {
        var w = make({ mode: "heat", target_temp: 20 })
        compare(text(w, "setpointText"), "20°")
        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", target_temp: 20.5 })))
        compare(text(w, "setpointText"), "20" + point + "5°")
    }

    // --- row 1 and the rest of the item ------------------------------------------------

    function test_roomTemperatureAndName() {
        var w = make({ mode: "cool", current_temp: 25.4 })
        compare(text(w, "itemName"), "Clim salon")
        compare(text(w, "roomTemp"), "25" + point + "4°")
        compare(find(w, "roomTemp").color, Theme.whiteColor)
    }

    function test_roomTemperatureAbsentLeavesNothing() {
        var w = make({ mode: "cool", current_temp: undefined })
        compare(find(w, "roomTemp").visible, false)
    }

    function test_nameIsYellowOnlyWhileTheDeviceActs() {
        compare(make({ mode: "heat", action: "heating" }).children.length > 0, true)
        compare(find(make({ mode: "heat", action: "heating" }), "itemName").color, Theme.yellowColor)
        compare(find(make({ mode: "cool", action: "cooling" }), "itemName").color, Theme.yellowColor)
        compare(find(make({ mode: "auto", action: "idle" }), "itemName").color, Theme.blueColor)
        compare(find(make({ mode: "off", action: "idle" }), "itemName").color, Theme.blueColor)
        compare(find(make({ mode: "heat", action: "heating", connected: false }), "itemName").color, Theme.blueColor)
    }

    function test_nameTurnsYellowWhenTheActionStarts() {
        var w = make({ mode: "heat", action: "idle" })
        compare(find(w, "itemName").color, Theme.blueColor)
        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", action: "heating" })))
        compare(find(w, "itemName").color, Theme.yellowColor)
    }

    function test_theItemIsTwoRowsHighAndHasNoPlusMinus() {
        var w = make({ mode: "heat" })
        compare(w.height, Units.dp(68))
        compare(find(w, "chevron").visible, true)
        compare(find(w, "chevron").name, "chevronR")
        // the -/+ of the controls only exist inside the closed settings
        compare(find(w, "settings").visible, false)
        compare(find(w, "btPlus"), null)
    }

    function test_dialTakesTheHeightOfTheRows() {
        var w = make({ mode: "heat" })
        var dial = find(w, "dial")
        verify(dial.height > Units.dp(40))
        verify(dial.height <= w.height)
        compare(dial.size, Units.dp(52))
    }

    // --- optional data of CLIM-6 ---------------------------------------------------------

    function test_aDeviceWithoutTheNewFieldsStillWorks() {
        var w = make({ mode: "cool" })
        compare(text(w, "statusText"), "Cooling")
        compare(text(w, "setpointText"), "22°")
    }

    function test_humidityIsOptionalAndNeverShownInTheList() {
        var texts = ["modeText", "statusText", "setpointText", "roomTemp"]
        var ref = texts.map(function(n) { return text(make({ mode: "cool" }), n) })
        var variants = [48, "wet", null, { x: 1 }, -5]
        for (var i = 0; i < variants.length; i++) {
            var w = make({ mode: "cool", humidity: variants[i] })
            compare(texts.map(function(n) { return text(w, n) }), ref)
        }
    }

    function test_malformedNewFieldsAreIgnored() {
        var w = make({ mode: "heat", action: "heating", connected: "no", fault: 7, lockout: "soon" })
        compare(text(w, "statusText"), "Heating")
        w = make({ mode: "heat", action: "heating", lockout: { remaining: "140" } })
        compare(text(w, "statusText"), "Heating")
        w = make({ mode: "heat", action: "heating", lockout: { remaining: -3 } })
        compare(text(w, "statusText"), "Heating")
    }

    function test_noDeviceObjectShowsOnlyTheMode() {
        var w = createTemporaryObject(itemComponent, tc, { modelData: harness.makeClimate(
            { id: "x", name: "Old climate", gui_type: "climate", state: "heat" }) })
        compare(text(w, "itemName"), "Old climate")
        compare(text(w, "modeText"), "Heat")
        compare(text(w, "statusText"), "")
        compare(text(w, "setpointText"), "--")
        compare(find(w, "roomTemp").visible, false)
    }

    // --- tap -------------------------------------------------------------------------------

    function test_tapOpensTheCurrentSettingsAndTapsAgainClosesThem() {
        var w = make({ mode: "heat", action: "heating" })
        var closed = w.height
        compare(find(w, "modeRow"), null)

        mouseClick(w, w.width / 2, Units.dp(30))
        compare(w.expanded, true)
        compare(find(w, "modeRow").visible, true)
        compare(find(w, "setpoint").visible, true)
        tryVerify(function() { return w.height > closed })

        mouseClick(w, w.width / 2, Units.dp(30))
        compare(w.expanded, false)
        compare(find(w, "modeRow"), null)
        tryCompare(w, "height", closed)
    }

    function test_tappingTheListItemSendsNothing() {
        var w = make({ mode: "heat" })
        mouseClick(w, w.width / 2, Units.dp(30))
        mouseClick(w, w.width / 2, Units.dp(30))
        wait(50)
        compare(harness.sent(), [])
    }

    // --- what a tap reaches --------------------------------------------------------------

    function test_onlyTheListItemTogglesTheSettings() {
        var w = make({ mode: "heat", action: "heating" })
        mouseClick(w, w.width / 2, Units.dp(30))
        compare(w.expanded, true)
        tryVerify(function() { return w.height > Units.dp(120) })

        // the margin under the controls, the name of a row, a greyed button: none of them closes the panel
        mouseClick(w, w.width - 2, w.height - 3)
        mouseClick(w, 4, w.height - 20)
        compare(w.expanded, true)
        compare(find(w, "modeRow").visible, true)
    }

    function test_pressFeedbackFollowsTheListItemOnly() {
        var w = make({ mode: "heat" })
        mousePress(w, w.width / 2, Units.dp(30))
        compare(w.__inBorder.state, "pressed")
        mouseRelease(w, w.width / 2, Units.dp(30))
        compare(w.__inBorder.state, "released")
        compare(w.expanded, true)

        // a press on the controls does not light the item up
        tryVerify(function() { return w.height > Units.dp(120) })
        mousePress(w, w.width - 2, w.height - 3)
        compare(w.__inBorder.state, "released")
        mouseRelease(w, w.width - 2, w.height - 3)
    }

    Component {
        id: scrolledComponent
        Flickable {
            width: 400; height: 300
            contentWidth: 400; contentHeight: 1200
            property alias item: inner
            IOClimate { id: inner; width: 400; clock: tc.fakeClock }
        }
    }

    function test_aScrollCancelsThePressFeedback() {
        var st = mkState({ mode: "heat" })
        var io = harness.makeClimate({ id: "salon_ac", name: "Clim", gui_type: "climate", state: "heat",
                                       device: device(st) })
        var flick = createTemporaryObject(scrolledComponent, tc)
        flick.item.modelData = io
        var w = flick.item

        mousePress(flick, 200, Units.dp(30))
        compare(w.__inBorder.state, "pressed")
        // the flickable takes the grab once the finger has moved far enough
        for (var y = 30; y < 200; y += 20)
            mouseMove(flick, 200, y)
        tryCompare(w.__inBorder, "state", "released")
        mouseRelease(flick, 200, 200)
        compare(w.expanded, false)
    }

    // --- dial colours in auto ---------------------------------------------------------------

    function test_autoKnobsFollowTheStatus_data() {
        var y = Theme.yellowColor, c = Theme.blueColor
        return [
            { tag: "acting", over: { mode: "auto", action: "heating" }, knobs: [y, c] },
            { tag: "idle", over: { mode: "auto", action: "idle" }, knobs: [y, c] },
            { tag: "offline", over: { connected: false }, knobs: [Theme.mutedColor, Theme.mutedColor] },
            { tag: "off", over: { mode: "off" }, knobs: [Theme.whiteColor, Theme.whiteColor] },
            { tag: "unknown mode", over: { mode: "" }, knobs: [Theme.mutedColor, Theme.mutedColor] },
            { tag: "lockout", over: { action: "idle", lockout: { output: "heat", remaining: 140 } },
              knobs: [Theme.amberColor, Theme.amberColor] }
        ]
    }

    //A range-only device shows its range in every mode, so off and unknown have knobs too
    function test_autoKnobsFollowTheStatus(d) {
        var over = { mode: "heat", target_temp_low: 20, target_temp_high: 24 }
        for (var k in d.over)
            over[k] = d.over[k]
        var c = caps()
        delete c.target_temp
        var dev = { family: "climate", impl: "proxy", capabilities: c, state: mkState(over),
                    ios: { mode: "m", target_temp_low: "l", target_temp_high: "h" } }
        var w = createTemporaryObject(itemComponent, tc, { modelData: harness.makeClimate(
            { id: "x", name: "X", gui_type: "climate", state: over.mode, device: dev }) })
        var knobs = find(w, "dial").knobColors
        compare(knobs.length, 2)
        compare(knobs[0], d.knobs[0])
        compare(knobs[1], d.knobs[1])
    }

    // --- setpoint precision and separator -----------------------------------------------------

    function test_setpointFollowsTheStep_data() {
        return [
            { tag: "quarter", step: 0.25, target: 22.25, text: "22" + point + "25°" },
            { tag: "quarter, round value", step: 0.25, target: 22, text: "22°" },
            { tag: "half", step: 0.5, target: 20.5, text: "20" + point + "5°" },
            { tag: "tenth", step: 0.1, target: 20.3, text: "20" + point + "3°" },
            { tag: "whole degrees", step: 1, target: 21, text: "21°" }
        ]
    }

    function test_setpointFollowsTheStep(d) {
        var w = make({ mode: "heat", target_temp: d.target },
                     { target_temp: { min: 16, max: 30, step: d.step, unit: "°C" } })
        compare(text(w, "setpointText"), d.text)
    }

    function test_theRangeFollowsTheStepToo() {
        var w = make({ mode: "auto", target_temp_low: 20.25, target_temp_high: 24 },
                     { target_temp: { min: 16, max: 30, step: 0.25, unit: "°C" } })
        compare(text(w, "setpointText"), "20" + point + "25 – 24°")
    }

    // --- cost of a hidden panel and of an idle timer --------------------------------------------

    function test_theLockoutTimerOnlyRunsWhileTheLockoutIsShown() {
        var w = make({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        compare(find(w, "lockoutTimer").running, true)

        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", action: "idle", connected: false,
            lockout: { output: "heat", remaining: 140 } })))
        compare(find(w, "lockoutTimer").running, false)

        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", action: "idle", fault: "sensor_stale",
            lockout: { output: "heat", remaining: 140 } })))
        compare(find(w, "lockoutTimer").running, false)

        harness.pushFrame("salon_ac", undefined, device(mkState({ mode: "heat", action: "idle",
            lockout: { output: "heat", remaining: 140 } })))
        compare(find(w, "lockoutTimer").running, true)
    }

    function test_noSettingsAreBuiltForACollapsedItem() {
        var w = make({ mode: "heat" })
        compare(find(w, "settings").item, null)
        w.expanded = true
        verify(find(w, "settings").item !== null)
        verify(find(w, "modeRow") !== null)
        w.expanded = false
        compare(find(w, "settings").item, null)
    }

    // --- anti-aliasing on the GPU ----------------------------------------------------------------

    function test_shapesAskForTheCurveRenderer() {
        var w = make({ mode: "heat", target_temp: 21 })
        if (Shape.CurveRenderer === undefined)
            skip("this Qt has no curve renderer")
        compare(find(w, "dialShape").preferredRendererType, Shape.CurveRenderer)
        compare(find(w, "iconShape").preferredRendererType, Shape.CurveRenderer)
    }
}
