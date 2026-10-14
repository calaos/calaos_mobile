import QtQuick
import QtTest
import SharedComponents

//What the climate views send and show for a device: the commands, the drafts
//laid over the server state, and the rules that decide between them.
TestCase {
    id: tc
    name: "ClimateModel"
    when: windowShown
    visible: true
    width: 420
    height: 300

    readonly property string point: Qt.locale().decimalPoint

    property real fakeNow: 1000000
    function fakeClock() { return fakeNow }

    Component {
        id: modelComponent
        ClimateModel { clock: tc.fakeClock }
    }

    function caps() {
        return {
            modes: ["off", "heat", "cool", "auto"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "low", "high"],
            swing_modes: ["off", "vertical"],
            reports_action: true
        }
    }

    function mkState(over) {
        var st = { mode: "heat", current_temp: 21, target_temp: 22, target_temp_low: 20, target_temp_high: 24,
                   action: "heating", fan_mode: "auto", swing_mode: "off" }
        for (var k in over) {
            if (over[k] === undefined)
                delete st[k]
            else
                st[k] = over[k]
        }
        return st
    }

    function device(st, capsOver, ios) {
        var c = caps()
        for (var k in capsOver) {
            if (capsOver[k] === undefined)
                delete c[k]
            else
                c[k] = capsOver[k]
        }
        return { family: "climate", impl: "proxy", capabilities: c, state: st,
                 ios: ios !== undefined ? ios : { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } }
    }

    function make(over, capsOver, ios) {
        var st = mkState(over)
        var io = harness.makeClimate({ id: "salon_ac", name: "Clim salon", gui_type: "climate",
                                       state: st.mode === undefined ? "" : st.mode, rw: "true",
                                       device: device(st, capsOver, ios) })
        var m = createTemporaryObject(modelComponent, tc, { modelData: io })
        verify(m !== null)
        return m
    }

    function init() {
        harness.reset()
        fakeNow = 1000000
    }

    function frame(m, over, capsOver, ios) {
        var st = mkState(over)
        harness.pushFrame("salon_ac", st.mode, device(st, capsOver, ios))
    }

    function sent() { return harness.sent().map(function(c) { return JSON.parse(c) }) }

    function tap(m, which, dir) {
        m.step(which, dir)
        m.flush()
    }

    // --- one command for a burst of taps ----------------------------------------------

    function test_tapsMakeOneCommand() {
        var m = make({ mode: "heat" })
        m.step("target", 1)
        m.step("target", 1)
        m.step("target", 1)
        m.step("target", -1)
        // shown at once, sent once the user stops tapping
        compare(m.values.target, 23)
        compare(harness.sent(), [])
        tryVerify(function() { return harness.sent().length === 1 }, 2000)
        wait(600)
        compare(sent(), [{ target_temp: 23 }])
    }

    function test_theSetpointStopsAtTheBounds() {
        var m = make({ mode: "heat", target_temp: 30 })
        compare(m.canStep("target", 1), false)
        compare(m.canStep("target", -1), true)
        m.step("target", 1)
        m.flush()
        compare(harness.sent(), [])
        compare(m.values.target, 30)
    }

    function test_theSetpointStartsFromTheCurrentTemperatureWhenUnknown() {
        var m = make({ mode: "heat", current_temp: 19.2, target_temp: undefined })
        m.step("target", 1)
        m.flush()
        compare(sent(), [{ target_temp: 19.5 }])
    }

    function test_autoMovesOneEndPerCommand() {
        var m = make({ mode: "auto", target_temp_low: 19, target_temp_high: 24 })
        compare(m.setpointKind, "range")
        tap(m, "low", 1)
        tap(m, "high", -1)
        compare(sent(), [{ target_temp_low: 19.5 }, { target_temp_high: 23.5 }])
        compare(m.values.low, 19.5)
        compare(m.values.high, 23.5)
    }

    function test_minRangeRefusesTheInnerButtons() {
        var m = make({ mode: "auto", target_temp_low: 21, target_temp_high: 23 })
        compare(m.rangeAtStop, true)
        compare(m.canStep("low", 1), false)
        compare(m.canStep("high", -1), false)
        compare(m.canStep("low", -1), true)
        compare(m.canStep("high", 1), true)
        m.step("low", 1)
        m.step("high", -1)
        m.flush()
        compare(harness.sent(), [])
        compare(m.values.low, 21)
        compare(m.values.high, 23)
    }

    function test_leavingTheStopFreesTheInnerButtons() {
        var m = make({ mode: "auto", target_temp_low: 21, target_temp_high: 23 })
        tap(m, "high", 1)
        compare(m.rangeAtStop, false)
        compare(m.canStep("low", 1), true)
        compare(m.canStep("high", -1), true)
    }

    function test_noGapRequiredWhenThereIsNoMinRange() {
        var m = make({ mode: "auto", target_temp_low: 22, target_temp_high: 22 },
                     { target_temp: { min: 16, max: 30, step: 0.5 } })
        compare(m.rangeAtStop, false)
        // the ends may meet, not cross
        compare(m.canStep("low", 1), false)
        compare(m.canStep("low", -1), true)
        compare(m.canStep("high", 1), true)
        compare(m.canStep("high", -1), false)
    }

    function test_aRangeOnlyDeviceNeverSendsASingleTarget() {
        var m = make({ mode: "heat", target_temp_low: 19, target_temp_high: 24 },
                     { target_temp: undefined }, { mode: "m", target_temp_low: "l", target_temp_high: "h" })
        compare(m.setpointKind, "range")
        tap(m, "high", 1)
        var cmds = sent()
        compare(cmds, [{ target_temp_high: 24.5 }])
        verify(harness.sent()[0].indexOf("\"target_temp\"") < 0)
    }

    function test_nothingMovesWhenTheDeviceIsNotRunning_data() {
        return [
            { tag: "off", over: { mode: "off" } },
            { tag: "unknown mode", over: { mode: "" } },
            { tag: "offline", over: { connected: false } }
        ]
    }

    function test_nothingMovesWhenTheDeviceIsNotRunning(d) {
        var m = make(d.over)
        compare(m.canStep("target", 1), false)
        compare(m.canStep("target", -1), false)
        m.step("target", 1)
        m.flush()
        compare(harness.sent(), [])
    }

    function test_noSetpointNoButtons() {
        var m = make({ mode: "fan_only" })
        compare(m.setpointKind, "none")
        compare(m.canStep("target", 1), false)
    }

    // --- mode, fan and swing ------------------------------------------------------------

    function test_aModeIsSentAsThePlainString() {
        var m = make({ mode: "cool" })
        m.chooseMode("heat")
        compare(harness.sent(), ["heat"])
    }

    function test_theCurrentModeIsNotResent() {
        var m = make({ mode: "cool" })
        m.chooseMode("cool")
        compare(harness.sent(), [])
    }

    function test_aModeTheDeviceDoesNotOfferIsNotSent() {
        var m = make({ mode: "cool" })
        m.chooseMode("dry")
        compare(harness.sent(), [])
    }

    function test_fanAndSwingAreSentAtOnceAsAPartialUpdate() {
        var m = make({ mode: "cool" })
        m.commit({ fan_mode: "high" })
        compare(sent(), [{ fan_mode: "high" }])
        m.commit({ swing_mode: "vertical" })
        compare(sent()[1], { swing_mode: "vertical" })
        compare(m.shown("fan_mode"), "high")
    }

    function test_aSentValueStaysUntilTheServerAnswers() {
        var m = make({ mode: "cool", current_temp: 22 })
        m.commit({ fan_mode: "high" })
        // an unrelated update must not undo it
        frame(m, { mode: "cool", current_temp: 22.4 })
        compare(m.shown("fan_mode"), "high")
        frame(m, { mode: "cool", current_temp: 22.4, fan_mode: "high" })
        compare(m.shown("fan_mode"), "high")
        compare(m.inFlight, 0)
    }

    // --- several commands in flight -----------------------------------------------------------

    function test_threePlusThenAnOutsideChange() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        tap(m, "target", 1)
        tap(m, "target", 1)
        compare(sent(), [{ target_temp: 22.5 }, { target_temp: 23 }, { target_temp: 23.5 }])
        compare(m.inFlight, 3)
        compare(m.shown("target_temp"), 23.5)

        // somebody else set 25
        frame(m, { mode: "heat", target_temp: 25 })
        compare(m.inFlight, 0)
        compare(m.shown("target_temp"), 25)
        compare(m.values.target, 25)
    }

    function test_theAnswersOfTheThreePlusAreFollowedUntilTheLast() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        tap(m, "target", 1)
        tap(m, "target", 1)
        frame(m, { mode: "heat", target_temp: 22.5 })
        compare(m.shown("target_temp"), 23.5)
        compare(m.inFlight, 2)
        frame(m, { mode: "heat", target_temp: 23 })
        frame(m, { mode: "heat", target_temp: 23.5 })
        compare(m.inFlight, 0)
        compare(m.shown("target_temp"), 23.5)
    }

    function test_autoRangeLowTwiceHighThenOutsideValues() {
        var m = make({ mode: "auto", target_temp_low: 20, target_temp_high: 24 })
        tap(m, "low", 1)
        tap(m, "low", 1)
        tap(m, "high", 1)
        compare(m.values.low, 21)
        compare(m.values.high, 24.5)
        compare(m.inFlight, 3)

        frame(m, { mode: "auto", target_temp_low: 18, target_temp_high: 26 })
        compare(m.inFlight, 0)
        compare(m.values.low, 18)
        compare(m.values.high, 26)
    }

    function test_aValueSentTwiceKeepsTheLastDraft() {
        // + - + : 22.5, 22, 22.5
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        tap(m, "target", -1)
        tap(m, "target", 1)
        compare(sent(), [{ target_temp: 22.5 }, { target_temp: 22 }, { target_temp: 22.5 }])
        compare(m.inFlight, 3)

        frame(m, { mode: "heat", target_temp: 22.5 })
        compare(m.inFlight, 2)
        compare(m.values.target, 22.5)
        frame(m, { mode: "heat", target_temp: 22 })
        compare(m.inFlight, 1)
        compare(m.values.target, 22.5)
        frame(m, { mode: "heat", target_temp: 22.5 })
        compare(m.inFlight, 0)
        compare(m.values.target, 22.5)
    }

    function test_theFirstCommandRefusedKeepsTheOthers() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        tap(m, "target", 1)
        harness.pushRefusal("salon_ac")
        compare(m.inFlight, 1)
        compare(m.values.target, 23)
    }

    function test_aValueSentTwiceAndTheFirstRefused() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        tap(m, "target", -1)
        tap(m, "target", 1)
        harness.pushRefusal("salon_ac")
        compare(m.inFlight, 2)
        compare(m.values.target, 22.5)
    }

    function test_aRefusedOnlyCommandShowsTheServerAgain() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        compare(m.values.target, 22.5)
        harness.pushRefusal("salon_ac")
        compare(m.values.target, 22)
    }

    function test_theRefusalOfAnotherDeviceIsIgnored() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        harness.pushRefusal("another_io")
        compare(m.values.target, 22.5)
    }

    function test_aRefusedFanChoiceReverts() {
        var m = make({ mode: "cool" })
        m.commit({ fan_mode: "high" })
        harness.pushRefusal("salon_ac")
        compare(m.shown("fan_mode"), "auto")
    }

    function test_nothingHeardForAWhileShowsTheServer() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        compare(m.values.target, 22.5)
        tryCompare(m, "inFlight", 0, 4000)
        compare(m.values.target, 22)
    }

    function test_aPendingEditSurvivesAnOutsideChange() {
        var m = make({ mode: "heat", target_temp: 22 })
        m.step("target", 1)
        frame(m, { mode: "heat", target_temp: 25 })
        compare(m.values.target, 22.5)
    }

    // --- coming and going ------------------------------------------------------------------

    function test_aPendingEditIsSentWhenTheViewGoesAway() {
        var m = make({ mode: "heat", target_temp: 22 })
        m.step("target", 1)
        compare(harness.sent(), [])
        m.destroy()
        wait(50)
        compare(sent(), [{ target_temp: 22.5 }])
    }

    function test_nothingIsSentWhenNothingWasEdited() {
        var m = make({ mode: "heat" })
        m.destroy()
        wait(50)
        compare(harness.sent(), [])
    }

    function test_anotherDeviceStartsClean() {
        var m = make({ mode: "heat", target_temp: 22 })
        tap(m, "target", 1)
        var io2 = harness.makeClimate({ id: "other", name: "Other", gui_type: "climate", state: "heat",
                                        device: device(mkState({ target_temp: 19 })) })
        m.modelData = io2
        compare(m.inFlight, 0)
        compare(m.values.target, 19)
    }

    // --- what is shown ------------------------------------------------------------------------

    function test_theLockoutCountsDownInSeconds() {
        var m = make({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        compare(m.statusLabel(m.status, true), "Starts in 2 min 20 s")
        fakeNow += 25000
        tryCompare(m, "lockoutSeconds", 115, 3000)
        compare(m.statusLabel(m.status, true), "Starts in 1 min 55 s")
        fakeNow += 100000
        tryCompare(m, "lockoutSeconds", 15, 3000)
        compare(m.statusLabel(m.status, true), "Starts in 15 s")
    }

    function test_theListSaysMinutesOnly() {
        var m = make({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        compare(m.statusLabel(m.status, false), "Starts in 2 min")
    }

    function test_humidityIsOptional() {
        var withIt = make({ mode: "cool", humidity: 48 }, { humidity: true })
        compare(withIt.devState.humidity, 48)
        // the page only has something to say when the capability and the value are both there
        verify(withIt.caps.humidity)
        var none = make({ mode: "cool", humidity: 48 })
        compare(none.caps.humidity, false)
    }
}
