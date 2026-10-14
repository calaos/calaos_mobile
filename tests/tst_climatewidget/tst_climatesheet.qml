import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import "../../qml/mobile"

//The settings of a climate device on the phone: the bottom sheet, the sheets
//stacked on it, and what a tap in them sends.
TestCase {
    id: tc
    name: "ClimateSheet"
    when: windowShown
    visible: true
    width: 420
    height: 900

    readonly property string point: Qt.locale().decimalPoint

    property real fakeNow: 1000000
    function fakeClock() { return fakeNow }

    Component {
        id: hostComponent
        ClimateSheet { clock: tc.fakeClock }
    }

    function caps(over) {
        var c = {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "quiet", "low"],
            swing_modes: ["off", "vertical"],
            reports_action: true, humidity: true
        }
        for (var k in over) {
            if (over[k] === undefined) delete c[k]
            else c[k] = over[k]
        }
        return c
    }

    function mkState(over) {
        var st = { mode: "cool", current_temp: 25.4, target_temp: 22, target_temp_low: 20, target_temp_high: 24,
                   action: "cooling", fan_mode: "auto", swing_mode: "off", humidity: 48 }
        for (var k in over) {
            if (over[k] === undefined) delete st[k]
            else st[k] = over[k]
        }
        return st
    }

    function device(st, capsOver) {
        return { family: "climate", impl: "proxy", capabilities: caps(capsOver), state: st,
                 ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } }
    }

    function open(over, capsOver) {
        var st = mkState(over)
        var io = harness.makeClimate({ id: "salon_ac", name: "Clim salon", gui_type: "climate",
                                       state: st.mode, rw: "true", device: device(st, capsOver) })
        var host = createTemporaryObject(hostComponent, tc)
        host.openFor(io)
        tryVerify(function() { return host.opened }, 3000)
        tryVerify(function() { return find(Overlay.overlay, "bigDial") !== null && find(Overlay.overlay, "bigDial").width > 0 }, 3000)
        wait(300)
        return host
    }

    function frame(over, capsOver) {
        var st = mkState(over)
        harness.pushFrame("salon_ac", st.mode, device(st, capsOver))
    }

    function init() {
        harness.reset()
        fakeNow = 1000000
    }

    function cleanup() {
        // sheets belong to the window: wait for them to be gone before the next test
        tryVerify(function() { return Overlay.overlay.children.length < 1 || !anyOpen() }, 3000)
    }

    function anyOpen() {
        return false
    }

    function find(item, name) {
        if (item === null || item === undefined)
            return null
        if (item.objectName === name)
            return item
        var kids = item.children
        for (var i = 0; i < kids.length; i++) {
            var r = find(kids[i], name)
            if (r)
                return r
        }
        // a Popup keeps its content in a popup item that is a child of the overlay
        return null
    }

    function sent() { return harness.sent().map(function(c) { return JSON.parse(c) }) }

    function face(name) {
        var step = find(Overlay.overlay, name)
        verify(step !== null, "no " + name)
        return find(step, "face")
    }

    function press(name) {
        mouseClick(face(name))
    }

    function text(name) { return find(Overlay.overlay, name).text }

    // --- the detail sheet ----------------------------------------------------------------

    function test_opensWithTheDialAndTheSetpointLine() {
        var host = open({ mode: "cool" })
        compare(host.opened, true)
        compare(text("sheetName"), "Clim salon")
        compare(text("roomText"), "Room 25" + point + "4°")
        compare(text("statusWord"), "Cooling")
        compare(text("valueText"), "22°")
        compare(find(Overlay.overlay, "pillWord").text, "Cool")
        compare(find(Overlay.overlay, "pillIcon").name, "snow")
        compare(find(Overlay.overlay, "stopText").visible, false)
    }

    function test_theDialShowsTheStatusColours_data() {
        return [
            { tag: "cooling", over: { mode: "cool", action: "cooling" }, fill: Theme.blueColor, knobs: 1 },
            { tag: "heating", over: { mode: "heat", action: "heating" }, fill: Theme.yellowColor, knobs: 1 },
            { tag: "idle", over: { mode: "heat", action: "idle" }, fill: Theme.mutedColor, knobs: 1 },
            { tag: "off", over: { mode: "off", action: "idle" }, fill: Theme.mutedColor, knobs: 1 },
            { tag: "offline", over: { mode: "heat", connected: false }, fill: "#4a4a4a", knobs: 1 },
            { tag: "waiting", over: { mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } },
              fill: Theme.amberColor, knobs: 1 }
        ]
    }

    function test_theDialShowsTheStatusColours(d) {
        open(d.over)
        var dial = find(Overlay.overlay, "bigDial")
        compare(dial.fillColor, d.fill)
        compare(dial.model.knobs.length, d.knobs)
        compare(dial.model.fill !== null, true)
    }

    function test_autoDialHasTwoKnobsAndTwoPairs() {
        open({ mode: "auto", action: "heating", target_temp_low: 19.5, target_temp_high: 24.5 })
        var dial = find(Overlay.overlay, "bigDial")
        compare(dial.model.knobs.length, 2)
        compare(dial.model.range !== null, true)
        compare(dial.knobColors[0], Theme.yellowColor)
        compare(dial.knobColors[1], Theme.blueColor)
        compare(dial.rangeColor, Theme.yellowColor)
        compare(text("lowText"), "19" + point + "5°")
        compare(text("highText"), "24" + point + "5°")
        compare(find(Overlay.overlay, "btLowMin").visible, true)
        compare(find(Overlay.overlay, "btHighPlus").visible, true)
    }

    function test_theMeasureIsATickOnTheDial() {
        open({ mode: "cool", current_temp: 25.4 })
        compare(find(Overlay.overlay, "bigDial").model.tick !== null, true)
    }

    function test_theDialNeedsNoMeasure() {
        open({ mode: "cool", current_temp: undefined })
        compare(find(Overlay.overlay, "bigDial").model.tick, null)
        compare(find(Overlay.overlay, "roomText").visible, false)
    }

    function test_offlineTheRangeKnobsAreNeutral() {
        open({ mode: "auto", connected: false })
        var dial = find(Overlay.overlay, "bigDial")
        compare(dial.knobColors[0], Theme.mutedColor)
        compare(dial.knobColors[1], Theme.mutedColor)
        compare(text("statusWord"), "Offline")
    }

    // --- -/+ ------------------------------------------------------------------------------------

    function test_plusSendsTheNewSetpoint() {
        var host = open({ mode: "heat", action: "heating", target_temp: 20.5 })
        press("btPlus")
        press("btPlus")
        compare(text("valueText"), "21" + point + "5°")
        compare(harness.sent(), [])
        tryVerify(function() { return harness.sent().length === 1 }, 2000)
        compare(sent(), [{ target_temp: 21.5 }])
    }

    function test_minusAtTheLowerBound() {
        open({ mode: "heat", target_temp: 16 })
        press("btMin")
        wait(600)
        compare(harness.sent(), [])
        compare(text("valueText"), "16°")
    }

    function test_theInnerButtonsAreRefusedAtMinRange() {
        open({ mode: "auto", action: "idle", target_temp_low: 21, target_temp_high: 23 })
        compare(find(Overlay.overlay, "stopText").visible, true)
        compare(text("stopText"), "Minimum gap 2°")
        compare(find(Overlay.overlay, "btLowPlus").enabled, false)
        compare(find(Overlay.overlay, "btHighMin").enabled, false)
        compare(find(Overlay.overlay, "btLowMin").enabled, true)
        compare(find(Overlay.overlay, "btHighPlus").enabled, true)
        compare(find(Overlay.overlay, "btLowPlus").opacity < 1, true)

        press("btLowPlus")
        press("btHighMin")
        wait(600)
        compare(harness.sent(), [])

        // the outer ones still move the range
        press("btHighPlus")
        wait(600)
        compare(sent(), [{ target_temp_high: 23.5 }])
    }

    function test_theButtonsAreOffWhenTheDeviceDoesNotRun_data() {
        return [
            { tag: "off", over: { mode: "off" } },
            { tag: "offline", over: { mode: "heat", connected: false } },
            { tag: "unknown mode", over: { mode: "" } }
        ]
    }

    function test_theButtonsAreOffWhenTheDeviceDoesNotRun(d) {
        open(d.over)
        compare(find(Overlay.overlay, "btPlus").enabled, false)
        compare(find(Overlay.overlay, "btMin").enabled, false)
        press("btPlus")
        wait(600)
        compare(harness.sent(), [])
    }

    function test_threePlusThenAnOutsideChangeShowsTheOutsideValue() {
        open({ mode: "heat", action: "heating", target_temp: 22 })
        for (var i = 0; i < 3; i++) {
            press("btPlus")
            tryVerify(function() { return harness.sent().length === i + 1 }, 2000)
        }
        compare(text("valueText"), "23" + point + "5°")
        frame({ mode: "heat", action: "heating", target_temp: 25 })
        compare(text("valueText"), "25°")
    }

    function test_theLineFollowsTheServer() {
        open({ mode: "heat", action: "heating", target_temp: 22 })
        frame({ mode: "heat", action: "heating", target_temp: 24.5, current_temp: 23 })
        compare(text("valueText"), "24" + point + "5°")
        compare(text("roomText"), "Room 23°")
    }

    function test_theLockoutCountsInSeconds() {
        open({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 140 } })
        compare(text("statusWord"), "Starts in 2 min 20 s")
        fakeNow += 30000
        tryCompare(find(Overlay.overlay, "statusWord"), "text", "Starts in 1 min 50 s", 3000)
    }

    // --- stacked sheets --------------------------------------------------------------------------

    function test_theModePillOpensAChoiceOnTopOfTheSheet() {
        var host = open({ mode: "cool" })
        mouseClick(find(Overlay.overlay, "modePill"))
        tryVerify(function() { return host.modeOpened }, 3000)
        var rows = find(Overlay.overlay, "modeRows")
        compare(rows.choices.map(function(c) { return c.value }), ["off", "heat", "cool", "auto", "dry", "fan_only"])
        compare(rows.current, "cool")
        compare(find(rows, "rowCheck") !== null, true)
        // the sheet below stays
        compare(host.opened, true)
        compare(find(Overlay.overlay, "sheetName").visible, true)
    }

    function test_choosingAModeSendsItAndClosesOnlyTheChoice() {
        var host = open({ mode: "cool" })
        mouseClick(find(Overlay.overlay, "modePill"))
        tryVerify(function() { return host.modeOpened }, 3000)
        mouseClick(find(Overlay.overlay, "row_heat"))
        compare(harness.sent(), ["heat"])
        wait(500)
        compare(host.opened, true)
        var rows = find(Overlay.overlay, "modeRows")
        verify(rows === null || !rows.visible)
    }

    function test_theCurrentModeIsNotResent() {
        var host = open({ mode: "cool" })
        mouseClick(find(Overlay.overlay, "modePill"))
        tryVerify(function() { return host.modeOpened }, 3000)
        mouseClick(find(Overlay.overlay, "row_cool"))
        wait(100)
        compare(harness.sent(), [])
    }

    function test_theModePillIsGreyedOffline() {
        open({ mode: "heat", connected: false })
        var pill = find(Overlay.overlay, "modePill")
        compare(pill.disabled, true)
        compare(pill.opacity < 1, true)
        mouseClick(pill)
        wait(300)
        var rows = find(Overlay.overlay, "modeRows")
        verify(rows === null || !rows.visible)
    }

    function test_moreSettingsListWhatTheCapabilitiesAnnounce() {
        var host = open({ mode: "cool", fan_mode: "low", swing_mode: "off", humidity: 48 })
        mouseClick(find(Overlay.overlay, "moreLink"))
        tryVerify(function() { return host.moreOpened }, 3000)
        compare(find(Overlay.overlay, "fanRows").choices.map(function(c) { return c.value }), ["auto", "quiet", "low"])
        compare(find(Overlay.overlay, "fanRows").current, "low")
        compare(find(Overlay.overlay, "swingRows").current, "off")
        compare(text("humidityText"), "Humidity 48 %")
    }

    function test_aFanChoiceIsSentAtOnceAndTheSheetStaysOpen() {
        var host = open({ mode: "cool" })
        mouseClick(find(Overlay.overlay, "moreLink"))
        tryVerify(function() { return host.moreOpened }, 3000)
        mouseClick(find(Overlay.overlay, "row_quiet"))
        compare(sent(), [{ fan_mode: "quiet" }])
        compare(find(Overlay.overlay, "fanRows").current, "quiet")
        compare(find(Overlay.overlay, "fanRows").visible, true)
        compare(host.opened, true)
    }

    function test_onlyWhatIsAnnouncedShows() {
        var host = open({ mode: "cool", humidity: undefined }, { fan_modes: undefined, humidity: undefined })
        // swing alone still gives a page
        mouseClick(find(Overlay.overlay, "moreLink"))
        tryVerify(function() { return host.moreOpened }, 3000)
        compare(find(Overlay.overlay, "fanSection").visible, false)
        compare(find(Overlay.overlay, "humidityText").visible, false)
    }

    function test_noMoreLinkWhenThereIsNothingMore() {
        open({ mode: "cool", humidity: undefined }, { fan_modes: undefined, swing_modes: undefined, humidity: undefined })
        compare(find(Overlay.overlay, "moreLink").visible, false)
    }

    function test_humidityAloneStillGivesAPage() {
        open({ mode: "cool", humidity: 61 }, { fan_modes: undefined, swing_modes: undefined })
        compare(find(Overlay.overlay, "moreLink").visible, true)
    }

    // --- closing ----------------------------------------------------------------------------------

    function test_closingSendsWhatIsStillBeingTyped() {
        var host = open({ mode: "heat", action: "heating", target_temp: 20 })
        press("btPlus")
        compare(harness.sent(), [])
        host.close()
        // at once, not when the 400 ms of the burst are over
        compare(sent(), [{ target_temp: 20.5 }])
    }

    function test_closeClosesTheStackToo() {
        var host = open({ mode: "cool" })
        mouseClick(find(Overlay.overlay, "modePill"))
        tryVerify(function() { return host.modeOpened }, 3000)
        host.close()
        tryVerify(function() { return !host.opened && !host.modeOpened }, 3000)
    }

    function test_theSheetOpensAgainForAnotherDevice() {
        var host = open({ mode: "cool" })
        host.close()
        tryVerify(function() { return !host.opened }, 3000)
        var io2 = harness.makeClimate({ id: "other", name: "PAC chambre", gui_type: "climate", state: "heat",
                                        device: device(mkState({ mode: "heat", action: "heating", target_temp: 20.5 })) })
        host.openFor(io2)
        tryVerify(function() { return host.opened && text("sheetName") === "PAC chambre" }, 3000)
        compare(text("valueText"), "20" + point + "5°")
    }
}
