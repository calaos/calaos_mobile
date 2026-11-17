import QtQuick
import QtTest
import SharedComponents

//The program editor against a fake programClient: what it reads, what it
//writes and in which order, and what it shows after a failure.
TestCase {
    id: tc
    name: "TimeRangeEditor"
    when: windowShown
    visible: true
    width: 600
    height: 1400

    QtObject {
        id: fake
        signal finished(string requestId, bool ok, var answer, string error)
        signal timeRangeChanged(string ioId)
        property var calls: []
        property int seq: 0
        property bool online: true
        property int answered: 0

        function push(c) {
            if (!online)
                return ""
            c.rid = "r" + (++seq)
            calls = calls.concat([c])
            return c.rid
        }
        function getTimeRange(ioId) { return push({ action: "get_timerange", id: ioId }) }
        function setTimeRange(ioId, ranges, months) { return push({ action: "set_timerange", id: ioId, ranges: ranges }) }
        function getStates(items) { return push({ action: "get_state", items: items }) }
        function setState(id, value) { return push({ action: "set_state", id: id, value: value }) }
    }

    Component {
        id: editorComponent
        TimeRangeEditor {
            width: 560
            height: 1300
            client: fake
            day: 1
        }
    }

    SignalSpy {
        id: appliedSpy
        signalName: "applied"
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
        fake.online = true
        server = { ranges: { h1: [range(1, 6, 0, 8, 0)], h2: [] },
                   states: { hd: "19", h1sp: "21", h2sp: "18", cd: "26", c1sp: "24", coff: "false" } }
    }

    function cleanup() {
        harness.setLanguage("")
    }

    //Answers every request not answered yet; writes go through failWith.
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

    function make(props) {
        var p = props || {}
        if (!p.device)
            p.device = { ios: ios(2) }
        var e = createTemporaryObject(editorComponent, tc, p)
        verify(e !== null)
        return e
    }

    function writes(from) {
        return fake.calls.slice(from || 0).filter(function(c) {
            return c.action === "set_timerange" || c.action === "set_state"
        })
    }

    function test_loadsEveryLevelAndTheStates() {
        var e = make()
        verify(findChild(e, "loadingText").visible)
        var actions = fake.calls.map(function(c) { return c.action + ":" + (c.id || "") })
        compare(actions.sort(), ["get_state:", "get_timerange:c1", "get_timerange:h1", "get_timerange:h2"])
        var st = fake.calls.filter(function(c) { return c.action === "get_state" })[0]
        compare(st.items.sort(), ["c1sp", "cd", "coff", "h1sp", "h2sp", "hd"])
        answerAll()
        verify(!findChild(e, "loadingText").visible)
        verify(findChild(e, "segment_1_360") !== null)
        compare(findChild(e, "segment_1_360").Accessible.name, "Level 1, 06:00 to 08:00, 21°")
        compare(findChild(e, "levelCount").text, "Levels: 1 of 6")
    }

    function test_newSetpointWritesTheSetpointThenTheRanges() {
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.openNew(17 * 60 + 30, 22 * 60 + 30)
        var form = findChild(e, "slotForm")
        verify(form.visible)
        form.setpoint = 20.5
        form.submit()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].action, "set_state")
        compare(w[0].id, "h2sp")
        compare(w[0].value, "20.5")
        answerAll()
        w = writes(from)
        compare(w.length, 2)
        compare(w[1].action, "set_timerange")
        compare(w[1].id, "h2")
        compare(w[1].ranges.length, 1)
        compare(w[1].ranges[0].start_hour, "17")
        compare(w[1].ranges[0].start_min, "30")
        verify(!form.visible)
        verify(findChild(e, "segment_2_1050") !== null)
    }

    function test_slotPastMidnightIsWrittenOnTwoDays() {
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.openNew(0, 60)
        var form = findChild(e, "slotForm")
        form.setpoint = 21
        findChild(form, "startField").text = "23:00"
        findChild(form, "endField").text = "05:00"
        form.days = [7]
        form.submit()
        answerAll()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].id, "h1")
        var days = w[0].ranges.map(function(r) { return r.day + " " + r.start_hour + "-" + r.end_hour + ":" + r.end_min })
        compare(days.sort(), ["1 0-5:0", "1 6-8:0", "7 23-23:59"])
    }

    function test_seventhLevelIsRefusedBeforeAnyCall() {
        var e = make({ device: { ios: ios(6) } })
        for (var n = 1; n <= 6; n++) {
            server.ranges["h" + n] = [range(n, 6, 0, 7, 0)]
            server.states["h" + n + "sp"] = String(15 + n)
        }
        answerAll()
        compare(findChild(e, "levelCount").text, "Levels: 6 of 6")
        var from = fake.calls.length
        e.openNew(600, 660)
        var form = findChild(e, "slotForm")
        form.setpoint = 25
        form.submit()
        compare(fake.calls.length, from)
        compare(findChild(form, "formMessage").text, "6 levels maximum for Heating")
        verify(form.visible)
    }

    function test_coolingOff() {
        var e = make()
        answerAll()
        e.kind = "cool"
        var from = fake.calls.length
        var off = findChild(e, "coolOffButton")
        verify(off.visible)
        verify(findChild(e, "defaultDown").visible)
        off.trigger()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].id, "coff")
        compare(w[0].value, "true")
        answerAll()
        verify(!findChild(e, "defaultDown").visible)
        compare(findChild(e, "emptyText").text, "No slot in cooling. Default: off.")
        compare(findChild(e, "segment_0_0").Accessible.name, "Default, 00:00 to 24:00, off")
    }

    function test_copyDayAsksThenWritesEachLevel() {
        server.ranges.h2 = [range(3, 18, 0, 20, 0)]
        var e = make()
        answerAll()
        var from = fake.calls.length
        findChild(e, "copyButton").trigger()
        var copy = findChild(e, "copyGo")
        verify(!copy.enabled)
        findChild(e, "copyDay3").trigger()
        findChild(e, "copyDay4").trigger()
        verify(!findChild(e, "copyDay1").enabled)
        copy.trigger()
        compare(writes(from).length, 0)
        compare(findChild(e, "copyConfirmText").text, "Wednesday (1 slot) will be replaced by those of Monday.")
        findChild(e, "copyReplace").trigger()
        answerAll()
        var w = writes(from)
        compare(w.map(function(c) { return c.id }), ["h1", "h2"])
        compare(w[0].ranges.length, 3)
        compare(w[1].ranges.length, 0)
    }

    function test_failedCopyRereadsAndRetries() {
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.day = 1
        e.copyTo([2])
        answerAll(function(c) { return c.action === "set_timerange" ? "config_not_written" : "" })
        compare(findChild(e, "banner").Accessible.name, "Incomplete copy: not saved, try again")
        var reread = fake.calls.slice(from).filter(function(c) { return c.action === "get_timerange" })
        compare(reread.length, 3)
        var retry = findChild(e, "retryButton")
        verify(retry.visible)
        var before = fake.calls.length
        retry.trigger()
        answerAll()
        compare(writes(before).length, 1)
        compare(server.ranges.h1.length, 2)
        verify(!findChild(e, "banner").visible)
    }

    function test_emptySourceIsRefused() {
        var e = make()
        answerAll()
        e.day = 5
        findChild(e, "copyButton").trigger()
        verify(findChild(e, "copyEmpty").visible)
        verify(!findChild(e, "copyGo").enabled)
    }

    function test_timeRangeChangedRereadsTheLevel() {
        var e = make()
        answerAll()
        var from = fake.calls.length
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        fake.timeRangeChanged("h2")
        fake.timeRangeChanged("other")
        compare(fake.calls.slice(from).map(function(c) { return c.action + ":" + c.id }), ["get_timerange:h2"])
        answerAll()
        verify(findChild(e, "segment_2_720") !== null)
    }

    function test_movingALevelSwapsSetpointsAndRanges() {
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.openSegment({ start: 720, end: 780, level: 2, setpoint: 18 })
        compare(findChild(e, "levelText").text, "Level 2")
        verify(!findChild(e, "levelDown").enabled)
        findChild(e, "levelUp").trigger()
        answerAll()
        var w = writes(from).map(function(c) { return c.action + ":" + c.id })
        compare(w.slice(0, 2).sort().concat(w.slice(2)),
                ["set_state:h1sp", "set_state:h2sp", "set_timerange:h1", "set_timerange:h2"])
        compare(server.states.h2sp, "21")
        compare(server.states.h1sp, "18")
        compare(server.ranges.h1[0].start_hour, "12")
    }

    function test_offlineIsReadOnly() {
        fake.online = false
        var e = make()
        verify(!findChild(e, "addButton").enabled)
        verify(!findChild(e, "copyButton").enabled)
    }

    function test_simpleModeAppliesOneSlotOnly() {
        var e = make({ simpleMode: true, kind: "heat" })
        appliedSpy.target = e
        appliedSpy.clear()
        answerAll()
        var form = findChild(e, "slotForm")
        verify(form.visible)
        verify(!findChild(e, "dayBar").visible)
        compare(findChild(form, "saveButton").text, "Apply")
        verify(!findChild(form, "cancelButton").visible)
        form.setpoint = 21
        form.days = [2, 3]
        var from = fake.calls.length
        form.submit()
        answerAll()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].id, "h1")
        compare(appliedSpy.count, 1)
        verify(form.visible)
    }

    function test_frenchTexts() {
        verify(harness.setLanguage("fr"))
        var e = make()
        answerAll()
        e.kind = "cool"
        compare(findChild(e, "emptyText").text, "Aucun créneau en clim. Défaut : 26 °C.")
        compare(findChild(e, "copyButton").text, "Copier ce jour…")
        compare(findChild(e, "levelCount").text, "Niveaux : 0 sur 6")
        e.kind = "heat"
        compare(findChild(e, "segment_1_360").Accessible.name, "Niveau 1, 06:00 à 08:00, 21°")
    }
}
