import QtQuick
import QtTest
import SharedComponents

//The program editor drawn for real in its pages, in the kiosk dialog (1024x768,
//18 px margin) and in the phone full screen sheet (390x844), against a fake
//programClient. With PROGRAM_SHOTS_DIR set the images are written there for the
//UI review; without it the states are still driven and checked.
TestCase {
    id: tc
    name: "TimeRangeCaptures"
    when: windowShown
    visible: true
    width: 1100
    height: 900

    readonly property string shotsDir: harness.env("PROGRAM_SHOTS_DIR")
    readonly property var screens: ({ kiosk: { w: 1024, h: 768 }, phone: { w: 390, h: 844 } })

    QtObject {
        id: fake
        signal finished(string requestId, bool ok, var answer, string error)
        signal timeRangeChanged(string ioId)
        property var calls: []
        property int seq: 0
        property int answered: 0

        function push(c) {
            c.rid = "r" + (++seq)
            calls = calls.concat([c])
            return c.rid
        }
        function getTimeRange(ioId) { return push({ action: "get_timerange", id: ioId }) }
        function setTimeRange(ioId, ranges) { return push({ action: "set_timerange", id: ioId, ranges: ranges }) }
        function getStates(items) { return push({ action: "get_state", items: items }) }
        function setState(id, value) { return push({ action: "set_state", id: id, value: value }) }
    }

    Component {
        id: hostComponent
        Rectangle {
            id: host
            property bool phone: false
            property alias editor: ed
            color: "#080808"

            //Same frame as DialogClimate's programDialog on the kiosk
            Rectangle {
                id: frame
                anchors { fill: parent; margins: host.phone ? 0 : Units.dp(18) }
                color: host.phone ? Theme.backgroundColor : "#171717"
                border.color: host.phone ? "transparent" : Theme.borderColor
                radius: host.phone ? 0 : Units.dp(8)
            }

            TimeRangeEditor {
                id: ed
                anchors {
                    fill: frame
                    leftMargin: Units.dp(host.phone ? 12 : 14)
                    rightMargin: Units.dp(host.phone ? 12 : 14)
                    topMargin: Units.dp(host.phone ? 8 : 14)
                    bottomMargin: Units.dp(host.phone ? 8 : 14)
                }
                client: fake
                day: 1
                phone: host.phone
                title: "Living room"
                holdSetpoint: 21
                fontFamily: calaosFont.fontFamily
            }
        }
    }

    property var server: ({})

    function range(day, sh, sm, eh, em) {
        return { day: String(day), start_hour: String(sh), start_min: String(sm), start_sec: "0",
                 start_type: "0", start_offset: "0", end_hour: String(eh), end_min: String(em),
                 end_sec: "0", end_type: "0", end_offset: "0" }
    }

    function ios(heatLevels) {
        var out = { heat_default: "hd", cool_level1: "c1", cool_level1_setpoint: "c1sp",
                    cool_default: "cd", cool_default_off: "coff", program_enabled: "pe" }
        for (var n = 1; n <= heatLevels; n++) {
            out["heat_level" + n] = "h" + n
            out["heat_level" + n + "_setpoint"] = "h" + n + "sp"
        }
        return out
    }

    function init() {
        fake.calls = []
        fake.seq = 0
        fake.answered = 0
        server = { ranges: {}, states: { hd: "17", cd: "26", c1sp: "24", coff: "false", pe: "true" } }
    }

    //Weekday and evening periods on days 1 to 5, so that the week is not empty
    function fillLevels(count) {
        var setpoints = [21, 19, 22, 18, 20, 23]
        var starts = [[6, 0, 8, 0], [12, 0, 13, 30], [17, 0, 19, 0], [19, 0, 20, 0], [20, 0, 22, 0], [22, 0, 23, 0]]
        for (var n = 1; n <= count; n++) {
            var s = starts[n - 1]
            server.ranges["h" + n] = [1, 2, 3, 4, 5].map(function(d) { return range(d, s[0], s[1], s[2], s[3]) })
            server.states["h" + n + "sp"] = String(setpoints[n - 1])
        }
    }

    function answerAll(failWith) {
        while (fake.answered < fake.calls.length) {
            var c = fake.calls[fake.answered++]
            if (c.action === "get_timerange")
                fake.finished(c.rid, true, { months: "111111111111", ranges: server.ranges[c.id] || [] }, "")
            else if (c.action === "get_state")
                fake.finished(c.rid, true, server.states, "")
            else if (failWith && failWith(c))
                fake.finished(c.rid, false, {}, failWith(c))
            else {
                if (c.action === "set_timerange")
                    server.ranges[c.id] = c.ranges
                else
                    server.states[c.id] = c.value
                fake.finished(c.rid, true, { success: "true" }, "")
            }
        }
    }

    function make(mode) {
        var host = createTemporaryObject(hostComponent, tc, {
            width: screens[mode].w, height: screens[mode].h, phone: mode === "phone" })
        verify(host !== null)
        return host
    }

    function open(mode, props) {
        var p = props || {}
        var host = make(mode)
        host.editor.device = { ios: ios(p.levels || 2) }
        if (p.simpleMode)
            host.editor.simpleMode = true
        if (p.readOnly)
            host.editor.readOnly = true
        host.editor.reload()
        answerAll()
        return host
    }

    function settle(host) {
        tryCompare(findChild(host.editor, "programStack"), "busy", false)
    }

    function openDay(host, d) {
        host.editor.openDay(d)
        settle(host)
        compare(host.editor.page, "day")
    }

    function shot(host, mode, name) {
        settle(host)
        waitForRendering(host)
        var img = grabImage(host)
        compare(img.width, screens[mode].w)
        compare(img.height, screens[mode].h)
        if (shotsDir !== "")
            img.save(shotsDir + "/program-" + mode + "-" + name + ".png")
        return img
    }

    function modes() { return [{ tag: "kiosk" }, { tag: "phone" }] }

    function test_week_data() { return modes() }
    function test_week(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        compare(host.editor.page, "week")
        verify(findChild(host.editor, "dayRow1").visible)
        verify(findChild(host.editor, "dayRow7").visible)
        shot(host, data.tag, "week")
    }

    function test_day_data() { return modes() }
    function test_day(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        openDay(host, 1)
        verify(findChild(host.editor, "addButton").visible)
        verify(findChild(host.editor, "restOfTime").visible)
        shot(host, data.tag, "day")
    }

    function test_emptyDay_data() { return modes() }
    function test_emptyDay(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        openDay(host, 7)
        verify(findChild(host.editor, "emptyDay").visible)
        shot(host, data.tag, "day-empty")
    }

    function test_paused_data() { return modes() }
    function test_paused(data) {
        fillLevels(3)
        server.states.pe = "false"
        var host = open(data.tag, { levels: 3 })
        verify(findChild(host.editor, "pausedBanner").visible)
        shot(host, data.tag, "week-paused")
    }

    function test_period_data() { return modes() }
    function test_period(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        openDay(host, 1)
        host.editor.openSegment({ start: 720, end: 810, level: 2, setpoint: 19 })
        verify(findChild(host.editor, "slotForm").visible)
        shot(host, data.tag, "period")
    }

    function test_simpleMode_data() { return modes() }
    function test_simpleMode(data) {
        fillLevels(1)
        var host = open(data.tag, { simpleMode: true, levels: 1 })
        var form = findChild(host.editor, "slotForm")
        verify(form.visible)
        compare(findChild(form, "saveButton").text, "Apply")
        shot(host, data.tag, "simple")
    }

    function test_seventhSetpointRefused_data() { return modes() }
    function test_seventhSetpointRefused(data) {
        fillLevels(6)
        var host = open(data.tag, { levels: 6 })
        openDay(host, 1)
        var from = fake.calls.length
        host.editor.openNew(600, 660)
        var form = findChild(host.editor, "slotForm")
        form.setpoint = 25
        form.submit()
        compare(fake.calls.length, from)
        compare(findChild(form, "formMessage").text, "6 different setpoints maximum for Heating")
        shot(host, data.tag, "seventh-refused")
    }

    function test_coolingOff_data() { return modes() }
    function test_coolingOff(data) {
        fillLevels(1)
        server.ranges.c1 = [1, 2, 3, 4, 5].map(function(d) { return range(d, 13, 0, 17, 0) })
        var host = open(data.tag, { levels: 1 })
        host.editor.kind = "cool"
        openDay(host, 1)
        findChild(host.editor, "coolOffButton").trigger()
        answerAll()
        verify(findChild(host.editor, "coolOffButton").checked)
        verify(!findChild(host.editor, "defaultStepper").visible)
        shot(host, data.tag, "day-cool-off")
    }

    function test_copyConfirmation_data() { return modes() }
    function test_copyConfirmation(data) {
        fillLevels(3)
        server.ranges.h2 = server.ranges.h2.concat([range(3, 18, 0, 20, 0)])
        var host = open(data.tag, { levels: 3 })
        openDay(host, 1)
        var from = fake.calls.length
        findChild(host.editor, "copyButton").trigger()
        settle(host)
        findChild(host.editor, "copyDay3").trigger()
        findChild(host.editor, "copyDay6").trigger()
        findChild(host.editor, "copyGo").trigger()
        compare(fake.calls.length, from)
        verify(findChild(host.editor, "copyConfirm").visible)
        compare(findChild(host.editor, "copyConfirmText").text, "Wednesday (4 periods) will be replaced by those of Monday.")
        shot(host, data.tag, "copy-confirm")
    }

    function test_failureBannerWithRetry_data() { return modes() }
    function test_failureBannerWithRetry(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        openDay(host, 1)
        host.editor.copyTo([2, 3])
        answerAll(function(c) { return c.action === "set_timerange" ? "config_not_written" : "" })
        verify(findChild(host.editor, "banner").visible)
        verify(findChild(host.editor, "retryButton").visible)
        shot(host, data.tag, "failure-retry")
    }

    function test_readFailedBannerWithRetry_data() { return modes() }
    function test_readFailedBannerWithRetry(data) {
        fillLevels(3)
        var host = make(data.tag)
        host.editor.device = { ios: ios(3) }
        host.editor.reload()
        while (fake.answered < fake.calls.length) {
            var c = fake.calls[fake.answered++]
            if (c.action === "get_timerange")
                fake.finished(c.rid, false, {}, "timeout")
            else
                fake.finished(c.rid, true, server.states, "")
        }
        compare(host.editor.banner, "Could not read the program, try again")
        verify(findChild(host.editor, "banner").visible)
        verify(findChild(host.editor, "retryButton").visible)
        shot(host, data.tag, "read-failed")
    }

    function test_readOnly_data() { return modes() }
    function test_readOnly(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3, readOnly: true })
        openDay(host, 1)
        verify(!findChild(host.editor, "addButton").enabled)
        shot(host, data.tag, "day-read-only")
    }
}
