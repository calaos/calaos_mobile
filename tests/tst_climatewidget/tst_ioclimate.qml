import QtQuick
import QtTest
import SharedComponents

TestCase {
    id: tc
    name: "IOClimate"
    when: windowShown
    visible: true
    width: 420
    height: 600

    readonly property string degC: "°C"
    readonly property string nbsp: " "

    Component {
        id: widgetComponent
        IOClimate { width: 400 }
    }

    // --- fixtures -----------------------------------------------------------

    function fullCaps() {
        return {
            modes: ["off", "heat", "cool", "auto"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: degC, min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "low", "high"],
            swing_modes: ["off", "vertical"],
            reports_action: true
        }
    }

    function fullState() {
        return {
            mode: "cool", current_temp: 23.4, target_temp: 21.0,
            target_temp_low: 19, target_temp_high: 24,
            action: "cooling", fan_mode: "auto", swing_mode: "off"
        }
    }

    function device(caps, state) {
        return { family: "climate", impl: "proxy", capabilities: caps, state: state, ios: {} }
    }

    function make(mode, dev) {
        var item = { id: "salon_ac", name: "Clim salon", gui_type: "climate", state: mode, rw: "true" }
        if (dev !== undefined)
            item.device = dev
        var io = harness.makeClimate(item)
        var w = createTemporaryObject(widgetComponent, tc, { modelData: io })
        verify(w !== null)
        return w
    }

    function makeFull(mode, over) {
        var caps = fullCaps(), st = fullState()
        st.mode = mode
        if (over && over.caps) for (var c in over.caps) caps[c] = over.caps[c]
        if (over && over.state) for (var s in over.state) st[s] = over.state[s]
        return make(mode, device(caps, st))
    }

    function init() {
        harness.reset()
    }

    // --- tree helpers ---------------------------------------------------------

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

    function chips(row) {
        var out = []
        for (var i = 0; i < row.children.length; i++) {
            var c = row.children[i]
            if (c.modelData && c.modelData.value !== undefined)
                out.push(c)
        }
        return out
    }

    function chipValues(row) {
        return chips(row).map(function(c) { return c.modelData.value })
    }

    function selectedValues(row) {
        return chips(row).filter(function(c) { return c.selected }).map(function(c) { return c.modelData.value })
    }

    function chip(row, value) {
        var m = chips(row).filter(function(c) { return c.modelData.value === value })
        verify(m.length === 1, "no chip " + value)
        return m[0]
    }

    function sentCommands() {
        return harness.sent()
    }

    function waitForCommands(n) {
        tryVerify(function() { return sentCommands().length >= n }, 2000)
        wait(50)
    }

    function frame(widget, state, dev) {
        harness.pushFrame("salon_ac", state, dev)
    }

    // --- what is shown --------------------------------------------------------

    function test_fullDevice() {
        var w = makeFull("cool")

        var modes = find(w, "modeRow")
        compare(modes.visible, true)
        compare(chipValues(modes), ["off", "heat", "cool", "auto"])
        compare(selectedValues(modes), ["cool"])
        compare(chip(modes, "heat").modelData.label, "Heat")

        compare(find(w, "currentTemp").text, "23.4" + nbsp + degC)
        var badge = find(w, "badge")
        compare(badge.visible, true)
        compare(badge.children[0].text, "Cooling")

        var sp = find(w, "setpoint")
        compare(sp.visible, true)
        compare(sp.active, true)
        compare(sp.ranged, false)
        compare(find(sp, "valueText").text, "21.0" + nbsp + degC)
        compare(find(sp, "slider").from, 16)
        compare(find(sp, "slider").to, 30)
        compare(find(sp, "slider").stepSize, 0.5)

        compare(find(w, "fanSection").visible, true)
        compare(chipValues(find(w, "fanRow")), ["auto", "low", "high"])
        compare(selectedValues(find(w, "fanRow")), ["auto"])
        compare(find(w, "swingSection").visible, true)
        compare(chipValues(find(w, "swingRow")), ["off", "vertical"])
    }

    function test_minimalDeviceShowsOnlyWhatItHas() {
        var w = make("heat", device({ modes: ["off", "heat"] }, { mode: "heat" }))

        compare(chipValues(find(w, "modeRow")), ["off", "heat"])
        compare(find(w, "setpoint").visible, false)
        compare(find(w, "fanSection").visible, false)
        compare(find(w, "swingSection").visible, false)
        compare(find(w, "badge").visible, false)
        compare(find(w, "currentTemp").text, "—")
    }

    function test_onlyFanOrOnlySwing() {
        var w = make("cool", device({ modes: ["cool"], fan_modes: ["low", "high"] }, { mode: "cool" }))
        compare(find(w, "fanSection").visible, true)
        compare(find(w, "swingSection").visible, false)

        var w2 = make("cool", device({ modes: ["cool"], swing_modes: ["both"] }, { mode: "cool" }))
        compare(find(w2, "fanSection").visible, false)
        compare(find(w2, "swingSection").visible, true)
    }

    function test_heightFollowsContent() {
        var full = makeFull("cool")
        var minimal = make("heat", device({ modes: ["off", "heat"] }, { mode: "heat" }))
        verify(full.height > minimal.height)
        verify(minimal.height > 0)
        var fanOnly = makeFull("fan_only")
        verify(fanOnly.height < full.height)
    }

    function test_actionIndicator_data() {
        return [
            { tag: "heating", action: "heating", text: "Heating" },
            { tag: "idle", action: "idle", text: "Idle" },
            { tag: "drying", action: "drying", text: "Drying" },
            { tag: "unknown action is shown humanized", action: "defrost_cycle", text: "Defrost cycle" }
        ]
    }

    function test_actionIndicator(d) {
        var w = makeFull("heat", { state: { action: d.action } })
        var badge = find(w, "badge")
        compare(badge.visible, true)
        compare(badge.children[0].text, d.text)
    }

    function test_actionHiddenWhenNotReportedOrAbsent() {
        var w = makeFull("heat", { caps: { reports_action: false } })
        compare(find(w, "badge").visible, false)

        var st = fullState()
        delete st.action
        var w2 = make("cool", device(fullCaps(), st))
        compare(find(w2, "badge").visible, false)
    }

    function test_absentValuesShowADash() {
        var st = { mode: "cool" }
        var w = make("cool", device(fullCaps(), st))
        compare(find(w, "currentTemp").text, "—")
        compare(find(find(w, "setpoint"), "valueText").text, "—")
        compare(selectedValues(find(w, "fanRow")), [])
    }

    function test_unitAndStepFollowTheCapability() {
        var caps = fullCaps()
        caps.target_temp = { min: 50, max: 86, step: 1, unit: "°F" }
        var w = make("heat", device(caps, { mode: "heat", current_temp: 68.04, target_temp: 70 }))
        compare(find(find(w, "setpoint"), "valueText").text, "70" + nbsp + "°F")
        compare(find(w, "currentTemp").text, "68" + nbsp + "°F")
    }

    // --- mode gating ----------------------------------------------------------

    function test_offGreysSetpointAndSelectorsButNotTheModes() {
        var w = makeFull("off")
        var sp = find(w, "setpoint")
        compare(sp.visible, true)
        compare(sp.active, false)
        compare(sp.enabled, false)
        verify(sp.opacity < 1)
        compare(find(w, "fanRow").active, false)
        compare(find(w, "swingRow").active, false)
        compare(selectedValues(find(w, "modeRow")), ["off"])

        // leaving "off" must stay possible
        mouseClick(chip(find(w, "modeRow"), "heat"))
        compare(sentCommands(), ["heat"])

        // a greyed control sends nothing
        harness.clearSent()
        mouseClick(chip(find(w, "fanRow"), "high"))
        mouseClick(find(sp, "btPlus"))
        wait(600)
        compare(sentCommands(), [])
    }

    function test_fanOnlyHidesTheSetpointButKeepsFanAndSwing() {
        var w = makeFull("fan_only", { caps: { modes: ["off", "fan_only", "cool"] } })
        compare(find(w, "setpoint").visible, false)
        compare(find(w, "fanSection").visible, true)
        compare(find(w, "fanRow").active, true)
        compare(chip(find(w, "modeRow"), "fan_only").modelData.label, "Fan only")
    }

    function test_unknownModeIsExplicit() {
        var w = makeFull("")
        compare(selectedValues(find(w, "modeRow")), [])
        var badge = find(w, "badge")
        compare(badge.visible, true)
        compare(badge.children[0].text, "Mode unknown")
        compare(find(w, "setpoint").active, false)
        compare(find(w, "fanRow").active, false)

        // choosing a mode is how to get out of it
        mouseClick(chip(find(w, "modeRow"), "cool"))
        compare(sentCommands(), ["cool"])
    }

    function test_modeUnknownToKnownWhenTheServerReports() {
        var w = makeFull("")
        compare(find(w, "badge").children[0].text, "Mode unknown")
        var st = fullState()
        frame(w, "heat", device(fullCaps(), st))
        compare(selectedValues(find(w, "modeRow")), ["heat"])
        compare(find(w, "setpoint").active, true)
        compare(find(w, "badge").children[0].text, "Cooling")
    }

    function test_autoWithRangeShowsTwoHandles() {
        var w = makeFull("auto")
        var sp = find(w, "setpoint")
        compare(sp.ranged, true)
        compare(find(sp, "valueText").text, "19.0 – 24.0" + nbsp + degC)
        compare(find(sp, "rangeSlider").visible, true)
        compare(find(sp, "slider").visible, false)
        compare(find(sp, "rangeSlider").first.value, 19)
        compare(find(sp, "rangeSlider").second.value, 24)
    }

    function test_autoWithoutRangeStaysSingle() {
        var caps = fullCaps()
        delete caps.target_temp_range
        var w = make("auto", device(caps, fullState()))
        var sp = find(w, "setpoint")
        compare(sp.ranged, false)
        compare(find(sp, "slider").visible, true)
        compare(find(sp, "valueText").text, "21.0" + nbsp + degC)
    }

    function test_rangeHandlesThatWereNeverReportedShowADash() {
        var st = { mode: "auto", target_temp: 21 }
        var w = make("auto", device(fullCaps(), st))
        compare(find(find(w, "setpoint"), "valueText").text, "—")
    }

    // --- tolerant decoding ----------------------------------------------------

    function test_noDeviceObject() {
        var w = make("cool")
        compare(find(w, "modeRow").visible, false)
        compare(find(w, "setpoint").visible, false)
        compare(find(w, "fanSection").visible, false)
        compare(find(w, "badge").visible, false)
        compare(find(w, "currentTemp").text, "Cool")
        compare(sentCommands(), [])
    }

    function test_noDeviceAndNoModeToo() {
        var w = make("")
        compare(find(w, "currentTemp").text, "—")
        compare(find(w, "badge").visible, false)
    }

    function test_malformedCapabilitiesDegradeGracefully() {
        var caps = {
            modes: "heat",
            target_temp: { min: 30, max: 16, step: "x" },
            fan_modes: [1, 2, null],
            swing_modes: {},
            reports_action: "yes"
        }
        var w = make("heat", device(caps, { mode: "heat", target_temp: 20 }))
        compare(find(w, "modeRow").visible, false)
        // inverted bounds fall back to the spec defaults instead of a broken slider
        var sp = find(w, "setpoint")
        compare(find(sp, "slider").from, 7)
        compare(find(sp, "slider").to, 35)
        compare(find(sp, "slider").stepSize, 0.5)
        compare(find(w, "fanSection").visible, false)
        compare(find(w, "swingSection").visible, false)
        compare(find(w, "badge").visible, false)
    }

    function test_malformedStateValues() {
        var st = { mode: "heat", current_temp: "warm", target_temp: "21", action: 3 }
        var w = make("heat", device(fullCaps(), st))
        compare(find(w, "currentTemp").text, "—")
        compare(find(find(w, "setpoint"), "valueText").text, "—")
        compare(find(w, "badge").visible, false)
    }

    // --- live updates ---------------------------------------------------------

    function test_followsTheServer() {
        var w = makeFull("cool")
        var st = fullState()
        st.mode = "heat"
        st.target_temp = 22.5
        st.current_temp = 20.1
        st.action = "heating"
        frame(w, "heat", device(fullCaps(), st))

        compare(selectedValues(find(w, "modeRow")), ["heat"])
        compare(find(find(w, "setpoint"), "valueText").text, "22.5" + nbsp + degC)
        compare(find(w, "currentTemp").text, "20.1" + nbsp + degC)
        compare(find(w, "badge").children[0].text, "Heating")
    }

    function test_deviceIsReplacedNotMerged() {
        var w = makeFull("cool")
        compare(find(w, "fanSection").visible, true)

        var caps = { modes: ["off", "cool"], target_temp: { min: 18, max: 26, step: 1, unit: degC } }
        frame(w, "cool", device(caps, { mode: "cool", target_temp: 22 }))

        compare(chipValues(find(w, "modeRow")), ["off", "cool"])
        compare(find(w, "fanSection").visible, false)
        compare(find(w, "swingSection").visible, false)
        compare(find(w, "badge").visible, false)
        compare(find(find(w, "setpoint"), "slider").from, 18)
        compare(find(find(w, "setpoint"), "valueText").text, "22" + nbsp + degC)
    }

    function test_deviceBeforeStateInOneFrameEndsConsistent() {
        var w = makeFull("cool")
        var st = fullState()
        st.mode = "heat"
        frame(w, "heat", device(fullCaps(), st))
        compare(selectedValues(find(w, "modeRow")), ["heat"])
        compare(find(w, "setpoint").active, true)
    }

    // --- commands -------------------------------------------------------------

    function test_modeButtonSendsThePlainModeString() {
        var w = makeFull("cool")
        mouseClick(chip(find(w, "modeRow"), "heat"))
        compare(sentCommands(), ["heat"])
    }

    function test_selectedModeIsNotResent() {
        var w = makeFull("cool")
        mouseClick(chip(find(w, "modeRow"), "cool"))
        wait(50)
        compare(sentCommands(), [])
    }

    function test_plusAndMinusTapsMakeOneCommand() {
        var w = makeFull("heat")
        var sp = find(w, "setpoint")
        mouseClick(find(sp, "btPlus"))
        mouseClick(find(sp, "btPlus"))
        mouseClick(find(sp, "btPlus"))
        mouseClick(find(sp, "btMin"))

        // shown immediately, sent once the user stops tapping
        compare(find(sp, "valueText").text, "22.0" + nbsp + degC)
        compare(sentCommands(), [])

        waitForCommands(1)
        wait(600)
        compare(sentCommands().length, 1)
        compare(JSON.parse(sentCommands()[0]), { target_temp: 22 })
    }

    function test_setpointStopsAtTheBounds() {
        var w = makeFull("heat", { state: { target_temp: 30 } })
        var sp = find(w, "setpoint")
        mouseClick(find(sp, "btPlus"))
        waitForCommands(1)
        compare(JSON.parse(sentCommands()[0]), { target_temp: 30 })
    }

    function test_sliderDragMakesOneCommand() {
        var w = makeFull("heat")
        var slider = find(find(w, "setpoint"), "slider")
        slider.value = 22
        slider.moved()
        slider.value = 23.5
        slider.moved()
        slider.value = 24.2
        slider.moved()

        waitForCommands(1)
        wait(600)
        compare(sentCommands().length, 1)
        compare(JSON.parse(sentCommands()[0]), { target_temp: 24 })
    }

    function test_sliderFollowingTheStateSendsNothing() {
        var w = makeFull("heat")
        var st = fullState()
        st.mode = "heat"
        st.target_temp = 25
        frame(w, "heat", device(fullCaps(), st))
        wait(600)
        compare(find(find(w, "setpoint"), "slider").value, 25)
        compare(sentCommands(), [])
    }

    function test_realSliderDrag() {
        var w = makeFull("heat")
        var slider = find(find(w, "setpoint"), "slider")
        mouseDrag(slider, slider.leftPadding + slider.availableWidth * 0.5, slider.height / 2,
                  slider.availableWidth * 0.3, 0)
        waitForCommands(1)
        wait(600)
        compare(sentCommands().length, 1)
        var cmd = JSON.parse(sentCommands()[0])
        verify(cmd.target_temp > 21 && cmd.target_temp <= 30)
        compare(cmd.target_temp * 2, Math.round(cmd.target_temp * 2))
    }

    function test_rangeKeepsMinRangeBetweenTheHandles() {
        var w = makeFull("auto")
        var rs = find(find(w, "setpoint"), "rangeSlider")
        rs.first.value = 23.5
        rs.first.moved()

        waitForCommands(1)
        compare(JSON.parse(sentCommands()[0]), { target_temp_low: 22, target_temp_high: 24 })
        // the handle goes back to the allowed position
        compare(rs.first.value, 22)
        compare(find(find(w, "setpoint"), "valueText").text, "22.0 – 24.0" + nbsp + degC)
    }

    function test_rangeHighHandle() {
        var w = makeFull("auto")
        var rs = find(find(w, "setpoint"), "rangeSlider")
        rs.second.value = 25.5
        rs.second.moved()
        waitForCommands(1)
        compare(JSON.parse(sentCommands()[0]), { target_temp_low: 19, target_temp_high: 25.5 })
    }

    function test_rangeHandleBlockedByMinRangeSnapsBack() {
        // the handle is dragged and clamps onto the value it already had:
        // nothing changes in the state, but the handle must still come back
        var w = makeFull("auto", { state: { target_temp_low: 22, target_temp_high: 24 } })
        var rs = find(find(w, "setpoint"), "rangeSlider")
        rs.first.value = 23.5
        rs.first.moved()
        compare(rs.first.value, 22)
    }

    function test_rangeUntouchedHandleNotReportedIsNotSent() {
        var st = { mode: "auto", target_temp_low: 19 }
        var w = make("auto", device(fullCaps(), st))
        var rs = find(find(w, "setpoint"), "rangeSlider")
        rs.first.value = 18
        rs.first.moved()
        waitForCommands(1)
        compare(JSON.parse(sentCommands()[0]), { target_temp_low: 18 })
    }

    function test_fanAndSwingSendAPartialUpdateImmediately() {
        var w = makeFull("cool")
        mouseClick(chip(find(w, "fanRow"), "high"))
        compare(sentCommands().length, 1)
        compare(JSON.parse(sentCommands()[0]), { fan_mode: "high" })
        mouseClick(chip(find(w, "swingRow"), "vertical"))
        compare(JSON.parse(sentCommands()[1]), { swing_mode: "vertical" })
    }

    function test_sentValueStaysUntilTheServerAnswers() {
        var w = makeFull("cool")
        mouseClick(chip(find(w, "fanRow"), "high"))
        compare(selectedValues(find(w, "fanRow")), ["high"])

        // an unrelated update (a new temperature) must not undo it
        var st = fullState()
        st.current_temp = 23.6
        frame(w, undefined, device(fullCaps(), st))
        compare(selectedValues(find(w, "fanRow")), ["high"])

        // the server answers with another value: it wins
        st.fan_mode = "low"
        frame(w, undefined, device(fullCaps(), st))
        compare(selectedValues(find(w, "fanRow")), ["high"])
        st.fan_mode = "high"
        frame(w, undefined, device(fullCaps(), st))
        compare(selectedValues(find(w, "fanRow")), ["high"])
    }

    function test_setpointIsShownAsClampedByTheServer() {
        var w = makeFull("heat")
        var sp = find(w, "setpoint")
        mouseClick(find(sp, "btPlus"))
        waitForCommands(1)
        compare(find(sp, "valueText").text, "21.5" + nbsp + degC)

        var st = fullState()
        st.mode = "heat"
        st.target_temp = 21.5
        frame(w, "heat", device(fullCaps(), st))
        compare(find(sp, "valueText").text, "21.5" + nbsp + degC)
    }

    function test_plusStartsFromTheCurrentTemperatureWhenTheTargetIsUnknown() {
        var st = { mode: "heat", current_temp: 19.2 }
        var w = make("heat", device(fullCaps(), st))
        mouseClick(find(find(w, "setpoint"), "btPlus"))
        waitForCommands(1)
        compare(JSON.parse(sentCommands()[0]), { target_temp: 19.5 })
    }

    // --- room views -------------------------------------------------------------

    Component {
        id: listComponent
        ItemListView { showHeader: false }
    }

    function test_roomListShowsAClimateItemWithTheClimateWidget() {
        var room = harness.makeRoom([
            { id: "salon_ac", name: "Clim salon", gui_type: "climate", state: "heat", visible: "true",
              device: device(fullCaps(), fullState()) },
            { id: "msg", name: "Message", gui_type: "var_string", state: "hello", visible: "true", rw: "true" }
        ])
        var list = createTemporaryObject(listComponent, tc, { model: room, width: 400, height: 500 })
        tryVerify(function() { return find(list.contentItem, "modeRow") !== null }, 2000)
        // one widget only: the other item keeps its own delegate
        compare(find(list.contentItem, "modeRow").visible, true)
    }
}
