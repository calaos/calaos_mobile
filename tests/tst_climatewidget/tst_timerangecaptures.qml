import QtQuick
import QtTest
import SharedComponents

//The program editor drawn for real in its states, at the desktop width and at
//the phone width (full screen sheet), against a fake programClient. With
//PROGRAM_SHOTS_DIR set the images are written there for the UI review; without
//it the states are still driven and checked.
TestCase {
    id: tc
    name: "TimeRangeCaptures"
    when: windowShown
    visible: true
    width: 760
    height: 1500

    readonly property string shotsDir: harness.env("PROGRAM_SHOTS_DIR")
    readonly property var widths: ({ desktop: 720, phone: 390 })

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
            property int editorWidth: 720
            property alias editor: ed
            color: Theme.backgroundColor
            //A phone sheet is the full screen width
            readonly property int margin: editorWidth < 500 ? 0 : 16
            width: editorWidth + 2 * margin
            height: 1400
            TimeRangeEditor {
                id: ed
                x: host.margin
                y: host.margin
                width: host.editorWidth
                height: host.height - 2 * host.margin
                client: fake
                day: 1
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
                    cool_default: "cd", cool_default_off: "coff" }
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
        server = { ranges: {}, states: { hd: "17", cd: "26", c1sp: "24", coff: "false" } }
    }

    //Weekday and evening slots on days 1 to 5, so that the week preview is not empty
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

    function flickableOf(e) {
        for (var i = 0; i < e.children.length; i++)
            if (e.children[i].contentHeight !== undefined)
                return e.children[i]
        return null
    }

    function open(mode, props) {
        var host = createTemporaryObject(hostComponent, tc, { editorWidth: widths[mode] })
        verify(host !== null)
        var p = props || {}
        host.editor.device = { ios: ios(p.levels || 2) }
        host.editor.phone = mode === "phone"
        if (p.simpleMode)
            host.editor.simpleMode = true
        host.editor.reload()
        answerAll()
        return host
    }

    //The window is sized to what the editor shows, a phone sheet to a phone screen
    function shot(host, mode, name, sheet) {
        host.height = 1400
        waitForRendering(host)
        var flick = flickableOf(host.editor)
        if (!sheet)
            tryVerify(function() { return flick.contentHeight > 200 })
        var h = sheet ? 800 : Math.ceil(flick.contentHeight) + 2 * host.margin
        host.height = h
        waitForRendering(host)
        var img = grabImage(host)
        verify(img.width > 0 && img.height > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/program-" + mode + "-" + name + ".png")
        return img
    }

    function modes() { return [{ tag: "desktop" }, { tag: "phone" }] }

    function test_simpleMode_data() { return modes() }
    function test_simpleMode(data) {
        fillLevels(1)
        var host = open(data.tag, { simpleMode: true, levels: 1 })
        var form = findChild(host.editor, "slotForm")
        verify(form.visible)
        compare(findChild(form, "saveButton").text, "Apply")
        verify(!findChild(host.editor, "dayBar").visible)
        shot(host, data.tag, "simple", data.tag === "phone")
    }

    function test_threeLevels_data() { return modes() }
    function test_threeLevels(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        compare(findChild(host.editor, "levelCount").text, "Levels: 3 of 6")
        shot(host, data.tag, "levels-3")
        host.editor.openSegment({ start: 720, end: 810, level: 2, setpoint: 19 })
        verify(findChild(host.editor, "slotForm").visible)
        shot(host, data.tag, "levels-3-edit", data.tag === "phone")
    }

    function test_sixLevelsRefuseTheSeventh_data() { return modes() }
    function test_sixLevelsRefuseTheSeventh(data) {
        fillLevels(6)
        var host = open(data.tag, { levels: 6 })
        compare(findChild(host.editor, "levelCount").text, "Levels: 6 of 6")
        shot(host, data.tag, "levels-6")
        var from = fake.calls.length
        host.editor.openNew(600, 660)
        var form = findChild(host.editor, "slotForm")
        form.setpoint = 25
        form.submit()
        compare(fake.calls.length, from)
        compare(findChild(form, "formMessage").text, "6 levels maximum for Heating")
        shot(host, data.tag, "levels-6-seventh-refused", data.tag === "phone")
    }

    function test_coolingOff_data() { return modes() }
    function test_coolingOff(data) {
        fillLevels(1)
        server.ranges.c1 = [1, 2, 3, 4, 5].map(function(d) { return range(d, 13, 0, 17, 0) })
        var host = open(data.tag, { levels: 1 })
        host.editor.kind = "cool"
        findChild(host.editor, "coolOffButton").trigger()
        answerAll()
        compare(findChild(host.editor, "defaultText").text, "Default: off")
        verify(!findChild(host.editor, "defaultDown").visible)
        shot(host, data.tag, "cool-off")
    }

    function test_copyConfirmation_data() { return modes() }
    function test_copyConfirmation(data) {
        fillLevels(3)
        server.ranges.h2 = server.ranges.h2.concat([range(3, 18, 0, 20, 0)])
        var host = open(data.tag, { levels: 3 })
        var from = fake.calls.length
        findChild(host.editor, "copyButton").trigger()
        findChild(host.editor, "copyDay3").trigger()
        findChild(host.editor, "copyDay6").trigger()
        findChild(host.editor, "copyGo").trigger()
        compare(fake.calls.length, from)
        verify(findChild(host.editor, "copyConfirm").visible)
        compare(findChild(host.editor, "copyConfirmText").text, "Wednesday (4 slots) will be replaced by those of Monday.")
        shot(host, data.tag, "copy-confirm")
    }

    function test_failureBannerWithRetry_data() { return modes() }
    function test_failureBannerWithRetry(data) {
        fillLevels(3)
        var host = open(data.tag, { levels: 3 })
        host.editor.copyTo([2, 3])
        answerAll(function(c) { return c.action === "set_timerange" ? "config_not_written" : "" })
        verify(findChild(host.editor, "banner").visible)
        verify(findChild(host.editor, "retryButton").visible)
        shot(host, data.tag, "failure-retry")
    }

    function test_readFailedBannerWithRetry_data() { return modes() }
    function test_readFailedBannerWithRetry(data) {
        fillLevels(3)
        var host = createTemporaryObject(hostComponent, tc, { editorWidth: widths[data.tag] })
        verify(host !== null)
        host.editor.device = { ios: ios(3) }
        host.editor.phone = data.tag === "phone"
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
}
