import QtQuick
import QtTest
import SharedComponents
import "../../qml/desktop"

//The settings of a climate device on the desktop: the dialog, the dialogs
//opened on top of it, and what a tap in them sends.
TestCase {
    id: tc
    name: "ClimateDialog"
    when: windowShown
    visible: true
    width: 1000
    height: 800

    readonly property string point: Qt.locale().decimalPoint

    property real fakeNow: 1000000
    onFakeNowChanged: harness.setNow(fakeNow)
    function fakeClock() { return fakeNow }

    //The layer main.qml gives the dialogs: they reparent themselves onto it
    OverlayLayer {
        id: layer
        objectName: "dialogOverlayLayer"
    }

    Component {
        id: dialogComponent
        DialogClimate { clock: tc.fakeClock }
    }

    function caps(over) {
        var c = {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "quiet", "low", "medium", "high"],
            swing_modes: ["off", "vertical", "horizontal", "both"],
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

    property var hosts: []

    function open(over, capsOver) {
        var st = mkState(over)
        var io = harness.makeClimate({ id: "salon_ac", name: "Clim salon", gui_type: "climate",
                                       state: st.mode, rw: "true", device: device(st, capsOver) })
        var dlg = createTemporaryObject(dialogComponent, tc)
        hosts.push(dlg)
        dlg.openFor(io)
        tryVerify(function() { return dlg.detail.showing && find(layer, "bigDial") !== null && dlg.detail.opacity === 1 }, 3000)
        wait(350)
        return dlg
    }

    function frame(over, capsOver) {
        var st = mkState(over)
        harness.pushFrame("salon_ac", st.mode, device(st, capsOver))
    }

    function cleanup() {
        for (var i = 0; i < hosts.length; i++)
            hosts[i].destroy()
        hosts = []
        wait(50)
    }

    function init() {
        harness.reset()
        fakeNow = 1000000
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
        return null
    }

    function sent() { return harness.sent().map(function(c) { return JSON.parse(c) }) }
    function text(name) { return find(layer, name).text }
    function press(name) { mouseClick(find(find(layer, name), "face")) }

    function shownDialogs() {
        var n = 0
        for (var i = 0; i < layer.children.length; i++) {
            if (layer.children[i].showing === true)
                n++
        }
        return n
    }

    // --- the dialog ------------------------------------------------------------------------

    function test_opensWithTheDialAndTheSetpointLine() {
        var dlg = open({ mode: "cool" })
        compare(dlg.detail.showing, true)
        compare(dlg.detail.title, "Clim salon")
        compare(text("roomText"), "Room 25" + point + "4°")
        compare(text("statusWord"), "Cooling")
        compare(text("valueText"), "22°")
        compare(find(layer, "pillWord").text, "Cool")
        compare(layer.currentOverlay, dlg.detail)
    }

    function test_theDialShowsTheStatusColours_data() {
        return [
            { tag: "cooling", over: { mode: "cool", action: "cooling" }, fill: Theme.blueColor },
            { tag: "heating", over: { mode: "heat", action: "heating" }, fill: Theme.yellowColor },
            { tag: "off", over: { mode: "off", action: "idle" }, fill: Theme.mutedColor },
            { tag: "offline", over: { mode: "heat", connected: false }, fill: "#4a4a4a" },
            { tag: "waiting", over: { mode: "heat", action: "idle", lockout: { output: "heat", remaining: 90 } }, fill: Theme.amberColor }
        ]
    }

    function test_theDialShowsTheStatusColours(d) {
        open(d.over)
        compare(find(layer, "bigDial").fillColor, d.fill)
    }

    function test_theDialIsSmallerThanOnThePhone() {
        open({ mode: "cool" })
        compare(find(layer, "bigDial").size, Units.dp(240))
    }

    function test_autoShowsTwoGroups() {
        open({ mode: "auto", action: "heating", target_temp_low: 19.5, target_temp_high: 24.5 })
        compare(find(layer, "bigDial").model.knobs.length, 2)
        compare(text("lowText"), "19" + point + "5°")
        compare(text("highText"), "24" + point + "5°")
        compare(find(layer, "btLowMin").visible, true)
    }

    function test_theInnerButtonsAreRefusedAtMinRange() {
        open({ mode: "auto", action: "idle", target_temp_low: 21, target_temp_high: 23 })
        compare(text("stopText"), "Minimum gap 2°")
        compare(find(layer, "btLowPlus").enabled, false)
        compare(find(layer, "btHighMin").enabled, false)
        press("btLowPlus")
        press("btHighMin")
        wait(600)
        compare(harness.sent(), [])
        press("btLowMin")
        wait(600)
        compare(sent(), [{ target_temp_low: 20.5 }])
    }

    function test_plusAndMinusMakeOneCommand() {
        open({ mode: "heat", action: "heating", target_temp: 20 })
        press("btPlus")
        press("btPlus")
        press("btPlus")
        press("btMin")
        compare(text("valueText"), "21°")
        tryVerify(function() { return harness.sent().length === 1 }, 2000)
        wait(600)
        compare(sent(), [{ target_temp: 21 }])
    }

    function test_threePlusThenAnOutsideChangeShowsTheOutsideValue() {
        open({ mode: "heat", action: "heating", target_temp: 22 })
        for (var i = 0; i < 3; i++) {
            press("btPlus")
            tryVerify(function() { return harness.sent().length === i + 1 }, 2000)
        }
        frame({ mode: "heat", action: "heating", target_temp: 25 })
        compare(text("valueText"), "25°")
    }

    function test_theLockoutCountsInSeconds() {
        open({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 90 } })
        compare(text("statusWord"), "Starts in 1 min 30 s")
        fakeNow += 45000
        tryCompare(find(layer, "statusWord"), "text", "Starts in 45 s", 3000)
    }

    // --- stacked dialogs ----------------------------------------------------------------------

    function test_theModePillOpensADialogOnTopOfTheFirst() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        var mode = find(layer, "modeDialog")
        tryVerify(function() { return mode.showing && mode.opacity === 1 }, 3000)
        wait(350)
        compare(shownDialogs(), 2)
        compare(layer.currentOverlay, mode)
        compare(mode.title, "Mode — Clim salon")
        compare(find(mode, "modeRows").current, "cool")
        compare(dlg.detail.showing, true)
    }

    function test_choosingAModeSendsItAndLeavesTheFirstDialogCurrent() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        var mode = find(layer, "modeDialog")
        tryVerify(function() { return mode.showing && mode.opacity === 1 }, 3000)
        wait(350)
        mouseClick(find(mode, "row_heat"))
        compare(harness.sent(), ["heat"])
        tryVerify(function() { return !mode.showing }, 3000)
        compare(dlg.detail.showing, true)
        // the layer gives its veil and its tap-to-dismiss back to the dialog that remains
        compare(layer.currentOverlay, dlg.detail)
        compare(shownDialogs(), 1)
    }

    function test_closeOnTheModeDialogGoesBackToTheFirst() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        var mode = find(layer, "modeDialog")
        tryVerify(function() { return mode.showing && mode.opacity === 1 }, 3000)
        wait(350)
        mouseClick(mode.negativeButton)
        tryVerify(function() { return !mode.showing }, 3000)
        compare(layer.currentOverlay, dlg.detail)
        compare(harness.sent(), [])
    }

    function test_theModePillIsGreyedOffline() {
        var dlg = open({ mode: "heat", connected: false })
        var pill = find(layer, "modePill")
        compare(pill.disabled, true)
        mouseClick(pill)
        wait(300)
        compare(dlg.modeDialog.showing, false)
    }

    function test_moreSettingsHasOneColumnPerCapability() {
        var dlg = open({ mode: "cool", fan_mode: "low", swing_mode: "off", humidity: 48 })
        mouseClick(find(layer, "moreLink"))
        var more = find(layer, "moreDialog")
        tryVerify(function() { return more.showing && more.opacity === 1 }, 3000)
        wait(350)
        compare(find(more, "more").columnCount, 2)
        compare(find(more, "fanRows").current, "low")
        compare(find(more, "swingRows").current, "off")
        compare(find(more, "humidityText").text, "Humidity 48 %")
        compare(dlg.detail.showing, true)
    }

    function test_aFanChoiceIsSentAtOnceAndTheDialogStaysOpen() {
        open({ mode: "cool" })
        mouseClick(find(layer, "moreLink"))
        var more = find(layer, "moreDialog")
        tryVerify(function() { return more.showing && more.opacity === 1 }, 3000)
        wait(350)
        mouseClick(find(more, "row_high"))
        compare(sent(), [{ fan_mode: "high" }])
        compare(find(more, "fanRows").current, "high")
        compare(more.showing, true)
    }

    function test_oneColumnWhenOnlyOneCapabilityIsThere() {
        open({ mode: "cool" }, { swing_modes: undefined })
        mouseClick(find(layer, "moreLink"))
        var more = find(layer, "moreDialog")
        tryVerify(function() { return more.showing && more.opacity === 1 }, 3000)
        wait(350)
        compare(find(more, "more").columnCount, 1)
        compare(find(more, "swingSection").visible, false)
    }

    function test_noMoreLinkWhenThereIsNothingMore() {
        open({ mode: "cool", humidity: undefined }, { fan_modes: undefined, swing_modes: undefined, humidity: undefined })
        compare(find(layer, "moreLink").visible, false)
    }

    // --- closing --------------------------------------------------------------------------------

    function test_closeSendsWhatIsStillBeingTyped() {
        var dlg = open({ mode: "heat", action: "heating", target_temp: 20 })
        press("btPlus")
        compare(harness.sent(), [])
        mouseClick(dlg.detail.negativeButton)
        // at once, not when the 400 ms of the burst are over
        compare(sent(), [{ target_temp: 20.5 }])
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        compare(layer.currentOverlay, null)
    }

    function test_aTapOutsideClosesTheTopDialogOnly() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        var mode = find(layer, "modeDialog")
        tryVerify(function() { return mode.showing && mode.opacity === 1 }, 3000)
        wait(350)
        mouseClick(layer, 5, 5)
        tryVerify(function() { return !mode.showing }, 3000)
        compare(dlg.detail.showing, true)
        mouseClick(layer, 5, 5)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
    }

    function test_theDialogOpensAgainForAnotherDevice() {
        var dlg = open({ mode: "cool" })
        mouseClick(dlg.detail.negativeButton)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        var io2 = harness.makeClimate({ id: "other", name: "PAC chambre", gui_type: "climate", state: "heat",
                                        device: device(mkState({ mode: "heat", action: "heating", target_temp: 20.5 })) })
        dlg.openFor(io2)
        tryVerify(function() { return dlg.detail.showing && dlg.detail.title === "PAC chambre" }, 3000)
        compare(text("valueText"), "20" + point + "5°")
    }

    // --- what stays reachable under a dialog ------------------------------------------------------

    function test_theDialogBelowCannotBeReachedThroughTheOneOnTop() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        tryVerify(function() { return dlg.modeDialog.showing && dlg.modeDialog.opacity === 1 }, 3000)
        wait(350)
        // the part of the first dialog that the mode dialog leaves uncovered: its Close button, on the right
        var close = dlg.detail.negativeButton
        var p = close.mapToItem(layer, close.width - 4, close.height / 2)
        verify(p.x > dlg.modeDialog.x + dlg.modeDialog.width, "the Close of the first dialog sticks out")
        mouseClick(layer, p.x, p.y)
        wait(400)
        // what was clicked was the veil: it closes the dialog on top and leaves the first one, current and alive
        compare(dlg.modeDialog.showing, false)
        compare(dlg.detail.showing, true)
        compare(layer.currentOverlay, dlg.detail)
    }

    function test_theDialogBelowWorksAgainOnceTheOnTopIsGone() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        tryVerify(function() { return dlg.modeDialog.showing && dlg.modeDialog.opacity === 1 }, 3000)
        wait(350)
        mouseClick(dlg.modeDialog.negativeButton)
        tryVerify(function() { return !dlg.modeDialog.showing }, 3000)
        compare(dlg.detail.enabled, true)
        mouseClick(dlg.detail.negativeButton)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        compare(layer.currentOverlay, null)
    }

    function test_rowsAreTheSameAfterAFrame() {
        var dlg = open({ mode: "cool" })
        mouseClick(find(layer, "modePill"))
        tryVerify(function() { return dlg.modeDialog.showing && dlg.modeDialog.opacity === 1 }, 3000)
        var before = find(dlg.modeDialog, "row_heat")
        verify(before !== null)
        frame({ mode: "cool", current_temp: 25.6 })
        verify(find(dlg.modeDialog, "row_heat") === before)
    }

    function test_offlineTheFanAndSwingRowsAreOff() {
        var dlg = open({ mode: "cool", connected: false })
        mouseClick(find(layer, "moreLink"))
        tryVerify(function() { return dlg.moreDialog.showing && dlg.moreDialog.opacity === 1 }, 3000)
        wait(350)
        compare(find(dlg.moreDialog, "fanRows").active, false)
        mouseClick(find(dlg.moreDialog, "row_high"))
        wait(100)
        compare(harness.sent(), [])
    }

    function test_offlineTheRangeValuesAreGrey() {
        open({ mode: "auto", connected: false })
        compare(find(layer, "lowText").color, Theme.mutedColor)
        compare(find(layer, "highText").color, Theme.mutedColor)
    }

    function test_theModelLetsGoOfTheDeviceWhenTheDialogCloses() {
        var dlg = open({ mode: "heat", action: "idle", lockout: { output: "heat", remaining: 90 } })
        compare(dlg.climate.lockoutTicking, true)
        mouseClick(dlg.detail.negativeButton)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        compare(dlg.climate.modelData, null)
        compare(dlg.climate.lockoutTicking, false)
    }
}
