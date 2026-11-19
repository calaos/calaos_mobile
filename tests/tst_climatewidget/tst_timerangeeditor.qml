import QtQuick
import QtTest
import SharedComponents

//The program editor against a fake programClient: its pages (Week, Day, form),
//what it reads, what it writes and in which order, and what it shows after a failure.
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
        function setTimeRange(ioId, ranges) { return push({ action: "set_timerange", id: ioId, ranges: ranges }) }
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

    SignalSpy {
        id: closeSpy
        signalName: "closeRequested"
    }

    property var server: ({})

    function range(day, sh, sm, eh, em) {
        return { day: String(day), start_hour: String(sh), start_min: String(sm), start_sec: "0",
                 start_type: "0", start_offset: "0", end_hour: String(eh), end_min: String(em),
                 end_sec: "0", end_type: "0", end_offset: "0" }
    }

    function ios(heatLevels, pause) {
        var out = { heat_default: "hd", cool_level1: "c1", cool_level1_setpoint: "c1sp",
                    cool_default: "cd", cool_default_off: "coff" }
        for (var n = 1; n <= heatLevels; n++) {
            out["heat_level" + n] = "h" + n
            out["heat_level" + n + "_setpoint"] = "h" + n + "sp"
        }
        if (pause)
            out.program_enabled = "pe"
        return out
    }

    function init() {
        fake.calls = []
        fake.seq = 0
        fake.answered = 0
        fake.online = true
        server = { ranges: { h1: [range(1, 6, 0, 8, 0)], h2: [] },
                   states: { hd: "19", h1sp: "21", h2sp: "18", cd: "26", c1sp: "24", coff: "false",
                             pe: "true" } }
    }

    function cleanup() {
        harness.setLanguage("")
    }

    //Answers every request not answered yet; writes go through failWith.
    function answerAll(failWith) {
        var err
        while (fake.answered < fake.calls.length) {
            var c = fake.calls[fake.answered++]
            if (c.action === "get_timerange")
                fake.finished(c.rid, true, { months: "111111111111", ranges: server.ranges[c.id] || [] }, "")
            else if (c.action === "get_state")
                fake.finished(c.rid, true, server.states, "")
            else if (failWith && (err = failWith(c)))
                fake.finished(c.rid, false, {}, err)
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

    //StackView swallows the mouse while a page slides in.
    function settle(e) {
        tryCompare(findChild(e, "programStack"), "busy", false)
    }

    function tap(e, name) {
        settle(e)
        var item = findChild(e, name)
        verify(item !== null, name)
        mouseClick(item)
        settle(e)
    }

    function writes(from) {
        return fake.calls.slice(from || 0).filter(function(c) {
            return c.action === "set_timerange" || c.action === "set_state"
        })
    }

    function header(e) {
        return [findChild(e, "backButton").text, findChild(e, "headerTitle").text]
    }

    //Every text and accessible name the user can see or hear under item.
    function shownTexts(item, out) {
        if (!item || !item.visible)
            return out
        if (typeof item.text === "string" && item.text !== "")
            out.push(item.text)
        if (item.Accessible && item.Accessible.name)
            out.push(item.Accessible.name)
        for (var i = 0; i < item.children.length; i++)
            shownTexts(item.children[i], out)
        return out
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
        compare(findChild(e, "dayRow1").Accessible.name, "Monday, 1 period")
        compare(findChild(e, "dayRow2").Accessible.name, "Tuesday, no period")
        compare(findChild(findChild(e, "weekBar1"), "segment_1_360").Accessible.name, "06:00 to 08:00, 21°")
    }

    function test_sameIosDoesNotReload() {
        var e = make()
        answerAll()
        var n = fake.calls.length
        e.device = { ios: ios(2), state: { current_temp: 21 } }
        compare(fake.calls.length, n)
        verify(!findChild(e, "loadingText").visible)
        e.device = { ios: ios(1) }
        verify(fake.calls.length > n)
    }

    //One level at a time, and the back button names the view it returns to.
    function test_weekDayFormAndBack() {
        var e = make({ title: "Living room" })
        closeSpy.target = e
        closeSpy.clear()
        answerAll()
        compare(e.page, "week")
        compare(header(e), ["Close", "Schedule"])
        compare(findChild(e, "headerSubtitle").text, "Living room")
        verify(!findChild(e, "copyButton").visible)

        tap(e, "dayRow1")
        compare(e.page, "day")
        compare(header(e), ["Week", "Monday"])
        compare(findChild(e, "headerSubtitle").text, "Living room · Heating · 1 period")
        verify(findChild(e, "copyButton").visible)

        tap(e, "period_360")
        compare(e.page, "form")
        compare(header(e), ["Monday", "Period"])
        verify(!findChild(e, "copyButton").visible)

        tap(e, "backButton")
        compare(e.page, "day")
        tap(e, "addButton")
        compare(header(e), ["Monday", "New period"])
        e.currentForm.cancelRequested()
        settle(e)
        compare(e.page, "day")
        tap(e, "backButton")
        compare(e.page, "week")
        compare(closeSpy.count, 0)
        tap(e, "backButton")
        compare(e.page, "week")
        compare(closeSpy.count, 1)

        tap(e, "dayRow4")
        compare(header(e), ["Week", "Thursday"])
        e.home()
        compare(e.page, "week")
    }

    function test_tappingTheDayBarDoesNothing() {
        var e = make()
        answerAll()
        e.openDay(1)
        var from = fake.calls.length
        tap(e, "dayBar")
        compare(e.page, "day")
        tap(e, "segment_1_360")
        compare(e.page, "day")
        compare(fake.calls.length, from)
    }

    function test_escapeGoesBackOneView() {
        var e = make()
        answerAll()
        e.forceActiveFocus()
        e.openDay(2)
        e.openNew(600, 660)
        settle(e)
        e.forceActiveFocus()
        keyClick(Qt.Key_Escape)
        settle(e)
        compare(e.page, "day")
        keyClick(Qt.Key_Escape)
        settle(e)
        compare(e.page, "week")
        //The root lets the key through: the host closes the window.
        closeSpy.target = e
        closeSpy.clear()
        keyClick(Qt.Key_Escape)
        compare(e.page, "week")
        compare(closeSpy.count, 0)
    }

    function test_periodListThenTheAddRowThenTheRestOfTheTime() {
        server.ranges.h2 = [range(1, 18, 0, 20, 30)]
        var e = make()
        answerAll()
        e.openDay(1)
        settle(e)
        var first = findChild(e, "period_360"), second = findChild(e, "period_1080")
        compare(findChild(first, "periodRange").text, "06:00 – 08:00")
        compare(findChild(first, "periodSetpoint").text, "21°")
        compare(second.Accessible.name, "18:00 – 20:30, 18°, edit")
        var add = findChild(e, "addButton"), rest = findChild(e, "restOfTime")
        verify(add.visible)
        verify(!findChild(e, "emptyDay").visible)
        verify(first.y < second.y && second.y < add.y && add.y < rest.y)
        compare(findChild(e, "defaultText").text, "19°")
        compare(findChild(e, "defaultText").Accessible.name, "Outside periods: 19°")
    }

    function test_emptyDayOffersTheFirstPeriod() {
        var e = make()
        answerAll()
        e.openDay(5)
        settle(e)
        verify(findChild(e, "emptyDay").visible)
        verify(!findChild(e, "addButton").visible)
        compare(findChild(e, "emptyText").text, "Heating stays at 19° all day.")
        tap(e, "addFirstButton")
        compare(e.page, "form")
        verify(!e.currentForm.editing)
        verify(!findChild(e, "deleteButton").visible)
    }

    function test_touchTargets() {
        var sizes = [{ phone: false, row: 56, target: 56 }, { phone: true, row: 48, target: 44 }]
        for (var i = 0; i < sizes.length; i++) {
            var e = make({ phone: sizes[i].phone })
            answerAll()
            compare(findChild(e, "dayRow1").height, sizes[i].row)
            verify(findChild(e, "backButton").height >= sizes[i].target)
            verify(findChild(e, "tab_heat").height >= sizes[i].target)
            e.openDay(1)
            settle(e)
            compare(findChild(e, "period_360").height, sizes[i].row)
            compare(findChild(e, "addButton").height, sizes[i].row)
            verify(findChild(e, "copyButton").height >= sizes[i].target)
            verify(findChild(e, "defaultDown").height >= sizes[i].target)
            verify(findChild(e, "defaultDown").width >= sizes[i].target)
            verify(findChild(e, "defaultUp").height >= sizes[i].target)
        }
    }

    function test_pauseShowsTheBannerAndDimsTheBars() {
        var e = make({ device: { ios: ios(2, true) }, holdSetpoint: 21 })
        answerAll()
        verify(findChild(e, "programStatus").visible)
        verify(!findChild(e, "pausedBanner").visible)
        compare(findChild(e, "weekBar1").opacity, 1)
        var from = fake.calls.length
        tap(e, "pauseButton")
        compare(writes(from).map(function(c) { return c.id + "=" + c.value }), ["pe=false"])
        answerAll()
        verify(e.paused)
        verify(!findChild(e, "programStatus").visible)
        verify(findChild(e, "pausedBanner").visible)
        compare(findChild(e, "pausedText").text, "Paused: setpoint stays at 21°")
        compare(findChild(e, "weekBar1").opacity, 0.5)
        e.openDay(1)
        compare(findChild(e, "dayBar").opacity, 0.5)
        e.home()
        from = fake.calls.length
        tap(e, "resumeButton")
        compare(writes(from).map(function(c) { return c.id + "=" + c.value }), ["pe=true"])
        answerAll()
        verify(!findChild(e, "pausedBanner").visible)
    }

    function test_noPauseWithoutTheProgramSwitch() {
        var e = make()
        answerAll()
        verify(!findChild(e, "programStatus").visible)
        verify(!findChild(e, "pausedBanner").visible)
    }

    function test_newSetpointWritesTheSetpointThenTheRanges() {
        var e = make()
        answerAll()
        e.openDay(1)
        var from = fake.calls.length
        e.openNew(17 * 60 + 30, 22 * 60 + 30)
        var form = e.currentForm
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
        compare(e.page, "day")
        verify(findChild(e, "period_1050") !== null)
    }

    function test_periodPastMidnightIsWrittenOnTwoDays() {
        var e = make()
        answerAll()
        e.openDay(7)
        var from = fake.calls.length
        e.openNew(0, 60)
        var form = e.currentForm
        form.setpoint = 21
        form.start = 23 * 60
        form.end = 5 * 60
        verify(findChild(form, "midnightBanner").visible)
        form.submit()
        answerAll()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].id, "h1")
        var days = w[0].ranges.map(function(r) { return r.day + " " + r.start_hour + "-" + r.end_hour + ":" + r.end_min })
        compare(days.sort(), ["1 0-5:0", "1 6-8:0", "7 23-23:59"])
        compare(e.page, "day")
    }

    function test_seventhSetpointIsRefusedBeforeAnyCall() {
        var e = make({ device: { ios: ios(6) } })
        for (var n = 1; n <= 6; n++) {
            server.ranges["h" + n] = [range(n, 6, 0, 7, 0)]
            server.states["h" + n + "sp"] = String(15 + n)
        }
        answerAll()
        var from = fake.calls.length
        e.openNew(600, 660)
        var form = e.currentForm
        form.setpoint = 25
        verify(findChild(form, "tooManyBanner").visible)
        compare(findChild(form, "tooManyBanner").Accessible.name, "6 different setpoints maximum")
        verify(!findChild(form, "saveButton").enabled)
        form.submit()
        compare(fake.calls.length, from)
        compare(e.page, "form")
        form.setpoint = 18
        verify(!findChild(form, "tooManyBanner").visible)
        verify(findChild(form, "saveButton").enabled)
    }

    function test_coolingOff() {
        var e = make()
        answerAll()
        e.kind = "cool"
        e.openDay(1)
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
        compare(findChild(e, "emptyText").text, "Cooling stays off all day.")
        compare(findChild(e, "segment_0_0").Accessible.name, "Default, 00:00 to 24:00, off")
    }

    function test_copySheetWritesEachLevelOnce() {
        server.ranges.h2 = [range(3, 18, 0, 20, 0)]
        var e = make()
        answerAll()
        e.openDay(1)
        var from = fake.calls.length
        tap(e, "copyButton")
        var copy = findChild(e, "copyGo")
        verify(findChild(e, "copySheet").visible)
        verify(!copy.enabled)
        verify(!findChild(e, "copyDay1").enabled)
        compare(findChild(e, "copyDay1").text, "Mon · source")
        compare(copy.text, "Copy to 0 days")
        findChild(e, "copyDay3").trigger()
        findChild(e, "copyDay4").trigger()
        verify(copy.enabled)
        compare(copy.text, "Copy to 2 days")
        verify(findChild(e, "copyWarning").visible)
        findChild(e, "copyWeekdays").trigger()
        compare(copy.text, "Copy to 4 days")
        findChild(e, "copyWeekend").trigger()
        compare(copy.text, "Copy to 2 days")
        findChild(e, "copyAll").trigger()
        compare(copy.text, "Copy to 6 days")
        findChild(e, "copyDay2").trigger()
        findChild(e, "copyDay5").trigger()
        findChild(e, "copyDay6").trigger()
        findChild(e, "copyDay7").trigger()
        compare(copy.text, "Copy to 2 days")
        compare(writes(from).length, 0)
        copy.trigger()
        verify(!findChild(e, "copySheet").visible)
        answerAll()
        var w = writes(from)
        compare(w.map(function(c) { return c.id }), ["h1", "h2"])
        compare(w[0].ranges.length, 3)
        compare(w[1].ranges.length, 0)
    }

    function test_copySheetClosesOnBackAndCancel() {
        var e = make()
        answerAll()
        e.openDay(1)
        tap(e, "copyButton")
        verify(findChild(e, "copySheet").visible)
        verify(e.back())
        verify(!findChild(e, "copySheet").visible)
        compare(e.page, "day")
        tap(e, "copyButton")
        findChild(e, "copyCancel").trigger()
        verify(!findChild(e, "copySheet").visible)
        compare(e.page, "day")
        compare(writes(0).length, 0)
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
        e.openDay(5)
        verify(!findChild(e, "copyButton").enabled)
        var from = fake.calls.length
        e.copyTo([2])
        compare(writes(from).length, 0)
        compare(findChild(e, "banner").Accessible.name, "Nothing to copy: this day has no period")
    }

    function test_timeRangeChangedRereadsTheLevel() {
        var e = make()
        answerAll()
        e.openDay(1)
        var from = fake.calls.length
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        fake.timeRangeChanged("h2")
        fake.timeRangeChanged("other")
        compare(fake.calls.slice(from).map(function(c) { return c.action + ":" + c.id }), ["get_timerange:h2"])
        answerAll()
        verify(findChild(e, "period_720") !== null)
        compare(findChild(e, "headerSubtitle").text, "Heating · 2 periods")
    }

    function test_overlapShowsThePriorityAndSwapsIt() {
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.openSegment({ start: 720, end: 780, level: 2, setpoint: 18 })
        var form = e.currentForm
        verify(form.editing)
        verify(!findChild(form, "overlapBanner").visible)
        form.end = 7 * 60
        form.start = 7 * 60 - 15
        var banner = findChild(form, "overlapBanner")
        verify(banner.visible)
        verify(/Level 1 comes before level 2/.test(banner.Accessible.name))
        form.swapped = true
        verify(/Level 1 comes before level 2/.test(banner.Accessible.name))
        form.submit()
        answerAll()
        var w = writes(from).map(function(c) { return c.action + ":" + c.id })
        compare(w.slice(0, 2).sort().concat(w.slice(2).sort()),
                ["set_state:h1sp", "set_state:h2sp", "set_timerange:h1", "set_timerange:h2"])
        compare(server.states.h1sp, "18")
        compare(server.states.h2sp, "21")
    }

    function swapScenario(failH2Once) {
        init()
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        var e = make()
        answerAll()
        e.openSegment({ start: 720, end: 780, level: 2, setpoint: 18 })
        var form = e.currentForm
        form.end = 7 * 60
        form.start = 7 * 60 - 15
        form.swapped = true
        var from = fake.calls.length
        var failed = false
        form.submit()
        answerAll(function(c) {
            if (failH2Once && !failed && c.action === "set_timerange" && c.id === "h2") {
                failed = true
                return "config_not_written"
            }
            return ""
        })
        return { e: e, from: from }
    }

    function sentOps(from) {
        return writes(from).map(function(c) { return c.id + ":" + JSON.stringify(c.ranges || c.value) }).sort()
    }

    function test_failedSwapRetriesTheSameOperations() {
        var okRun = swapScenario(false)
        var expected = JSON.stringify([server.ranges, server.states])
        var okOps = sentOps(okRun.from)

        var r = swapScenario(true)
        compare(r.e.page, "form")
        compare(findChild(r.e, "banner").Accessible.name, "Incomplete save: not saved, try again")
        var retry = findChild(r.e, "retryButton")
        verify(retry.visible)
        var before = fake.calls.length
        retry.trigger()
        answerAll()
        compare(sentOps(before), okOps)
        compare(JSON.stringify([server.ranges, server.states]), expected)
        compare(r.e.page, "week")
    }

    function test_failedSwapLocksTheFormUntilRetried() {
        var r = swapScenario(true)
        var form = r.e.currentForm
        verify(r.e.retryAction !== null)
        verify(!findChild(form, "saveButton").enabled)
        var before = fake.calls.length
        form.submit()
        form.start = 6 * 60
        form.submit()
        answerAll()
        compare(writes(before).length, 0)
        compare(r.e.page, "form")
        findChild(form, "cancelButton").trigger()
        compare(r.e.page, "week")
        verify(r.e.retryAction === null)
        compare(findChild(r.e, "banner").visible, false)
    }

    function test_deleteAsksInPlaceThenWrites() {
        server.ranges.h2 = [range(1, 12, 0, 13, 30)]
        var e = make()
        answerAll()
        var from = fake.calls.length
        e.openSegment({ start: 720, end: 810, level: 2, setpoint: 18 })
        var form = e.currentForm
        verify(findChild(form, "deleteButton").visible)
        findChild(form, "deleteButton").trigger()
        compare(writes(from).length, 0)
        compare(findChild(form, "deleteConfirm").text, "Delete the period 12:00 – 13:30?")
        findChild(form, "cancelButton").trigger()
        verify(!findChild(form, "deleteConfirm").visible)
        findChild(form, "deleteButton").trigger()
        findChild(form, "confirmDeleteButton").trigger()
        answerAll()
        compare(server.ranges.h2.length, 0)
        compare(e.page, "week")
    }

    function test_failedSaveKeepsTheInput() {
        var e = make()
        answerAll()
        e.openNew(600, 660)
        var form = e.currentForm
        form.start = 9 * 60 + 15
        form.setpoint = 21
        form.submit()
        answerAll(function(c) { return c.action === "set_timerange" ? "config_not_written" : "" })
        compare(e.page, "form")
        compare(form.start, 9 * 60 + 15)
        compare(findChild(e, "banner").Accessible.name, "Not saved, try again")
        verify(findChild(e, "retryButton").visible)
    }

    function test_scopeDeniedLocksTheEditor() {
        var e = make()
        answerAll()
        e.openNew(600, 660)
        var form = e.currentForm
        form.submit()
        answerAll(function(c) { return c.action === "set_timerange" || c.action === "set_state" ? "scope denied" : "" })
        verify(e.scopeDenied)
        compare(findChild(e, "banner").Accessible.name, "This account cannot change the program")
        verify(!findChild(e, "retryButton").visible)
        verify(!findChild(form, "saveButton").enabled)
    }

    function test_offlineIsReadOnly() {
        fake.online = false
        var e = make()
        verify(findChild(e, "offlineText").visible)
        verify(!findChild(e, "pauseButton").enabled)
        e.openDay(1)
        verify(!findChild(e, "addButton").enabled)
        verify(!findChild(e, "copyButton").enabled)
        verify(!findChild(e, "defaultUp").enabled)
    }

    function test_readOnlyHostIsReadOnly() {
        var e = make({ readOnly: true })
        answerAll()
        verify(findChild(e, "offlineText").visible)
        e.openDay(1)
        verify(!findChild(e, "addButton").enabled)
    }

    function test_simpleModeAppliesOnePeriodOnly() {
        var e = make({ simpleMode: true, kind: "heat" })
        appliedSpy.target = e
        appliedSpy.clear()
        answerAll()
        compare(e.page, "form")
        var form = findChild(e, "periodView")
        verify(form.visible)
        verify(findChild(e, "dayBar") === null)
        compare(findChild(form, "saveButton").text, "Apply")
        verify(!findChild(form, "cancelButton").visible)
        form.setpoint = 21
        var from = fake.calls.length
        form.submit()
        answerAll()
        var w = writes(from)
        compare(w.length, 1)
        compare(w[0].id, "h1")
        compare(appliedSpy.count, 1)
        verify(form.visible)
        verify(!e.back())
    }

    function chipTexts(e, from, to) {
        var out = []
        for (var i = from; i <= to; ++i) {
            var b = findChild(e, "copyDay" + i)
            out.push(b.text + "|" + b.Accessible.name)
        }
        return out
    }

    function test_copyChipsShowThreeLettersAndSpellTheDayOut() {
        var e = make()
        answerAll()
        e.openDay(1)
        e.openCopy()
        compare(chipTexts(e, 2, 7), ["Tue|Tuesday", "Wed|Wednesday", "Thu|Thursday", "Fri|Friday", "Sat|Saturday", "Sun|Sunday"])
        compare(findChild(e, "copyDay1").Accessible.name, "Monday, source")
    }

    function test_frenchCopyChips() {
        verify(harness.setLanguage("fr"))
        var e = make()
        answerAll()
        e.openDay(1)
        e.openCopy()
        compare(chipTexts(e, 2, 7), ["Mar|Mardi", "Mer|Mercredi", "Jeu|Jeudi", "Ven|Vendredi", "Sam|Samedi", "Dim|Dimanche"])
        compare(findChild(e, "copyGo").text, "Copier sur 0 jours")
        findChild(e, "copyAll").trigger()
        compare(findChild(e, "copyGo").text, "Copier sur 6 jours")
    }

    function test_germanCopyChips() {
        verify(harness.setLanguage("de"))
        var e = make()
        answerAll()
        e.openDay(1)
        e.openCopy()
        compare(chipTexts(e, 2, 7).map(function(t) { return t.split("|")[0] }), ["Di", "Mi", "Do", "Fr", "Sa", "So"])
    }

    function test_frenchTexts() {
        verify(harness.setLanguage("fr"))
        var e = make({ title: "Salon" })
        answerAll()
        compare(header(e), ["Fermer", "Programme"])
        compare(findChild(e, "dayRow1").Accessible.name, "Lundi, 1 plage")
        compare(findChild(e, "dayRow2").Accessible.name, "Mardi, aucune plage")
        e.openDay(1)
        compare(header(e), ["Semaine", "Lundi"])
        compare(findChild(e, "headerSubtitle").text, "Salon · Chauffage · 1 plage")
        compare(findChild(e, "copyButton").text, "Copier vers…")
        compare(findChild(e, "period_360").Accessible.name, "06:00 – 08:00, 21°, modifier")
        compare(findChild(e, "addButton").Accessible.name, "Ajouter une plage")
        e.openNew(600, 660)
        compare(header(e), ["Lundi", "Nouvelle plage"])
        e.home()
        e.kind = "cool"
        e.openDay(5)
        compare(findChild(e, "emptyText").text, "La clim reste à 26° toute la journée.")
    }

    function test_germanTexts() {
        verify(harness.setLanguage("de"))
        var e = make()
        answerAll()
        compare(header(e), ["Schließen", "Zeitplan"])
        compare(findChild(e, "dayRow1").Accessible.name, "Montag, 1 Zeitraum")
        e.openDay(1)
        compare(header(e), ["Woche", "Montag"])
        compare(findChild(e, "addButton").Accessible.name, "Zeitraum hinzufügen")
    }

    //The new vocabulary: periods, no slot nor level number, in the three languages.
    function test_noSlotNorLevelWording_data() {
        return [{ tag: "en", lang: "" }, { tag: "fr", lang: "fr" }, { tag: "de", lang: "de" }]
    }
    function test_noSlotNorLevelWording(data) {
        if (data.lang !== "")
            verify(harness.setLanguage(data.lang))
        server.ranges.h2 = [range(1, 12, 0, 13, 0)]
        var e = make({ device: { ios: ios(2, true) } })
        answerAll()
        var seen = shownTexts(e, [])
        e.openDay(1)
        settle(e)
        seen = shownTexts(e, seen)
        e.openDay(5)
        e.home()
        e.openDay(5)
        settle(e)
        seen = shownTexts(e, seen)
        e.openNew(1080, 1200)
        seen = shownTexts(e, seen)
        e.closeForm()
        e.openDay(1)
        e.openCopy()
        seen = shownTexts(e, seen)
        verify(seen.length > 20)
        var bad = seen.filter(function(t) {
            return /slot|level|niveau|créneau|Stufe|Zeitfenster|\bN-?\d/i.test(t)
        })
        compare(bad, [])
    }

    //get_timerange fails: the level looks empty, a write would erase the server's periods.
    function test_noWriteBeforeTheReadOrAfterItFailed() {
        var e = make()
        e.openDay(1)
        verify(!findChild(e, "addButton").enabled)
        verify(!findChild(e, "copyButton").enabled)
        var from = fake.calls.length
        e.applyRequest({ days: [1], start: 1080, end: 1200, setpoint: 20, original: null })
        compare(writes(from).length, 0)

        var first = fake.calls.filter(function(c) { return c.action === "get_timerange" })[0]
        fake.answered = fake.calls.length
        for (var i = 0; i < fake.calls.length; i++) {
            var c = fake.calls[i]
            if (c === first)
                fake.finished(c.rid, false, {}, "timeout")
            else if (c.action === "get_timerange")
                fake.finished(c.rid, true, { ranges: server.ranges[c.id] || [] }, "")
            else
                fake.finished(c.rid, true, server.states, "")
        }
        verify(findChild(e, "banner").visible)
        verify(findChild(e, "retryButton").visible)
        verify(!findChild(e, "addButton").enabled)
        verify(!findChild(e, "copyButton").enabled)
        verify(!findChild(e, "defaultUp").enabled)
        e.applyRequest({ days: [1], start: 1080, end: 1200, setpoint: 20, original: null })
        compare(writes(from).length, 0)

        from = fake.calls.length
        findChild(e, "retryButton").trigger()
        verify(from < fake.calls.length)
        answerAll()
        verify(!findChild(e, "banner").visible)
        verify(findChild(e, "addButton").enabled)
    }

    function test_checkableButtonsFollowTheirModelAfterSeveralTaps() {
        var e = make()
        answerAll()
        e.openDay(1)
        e.openCopy()
        findChild(e, "copyDay2").trigger()
        findChild(e, "copyDay2").trigger()
        findChild(e, "copyDay2").trigger()
        verify(findChild(e, "copyDay2").checked)
        findChild(e, "copyCancel").trigger()
        e.back()
        settle(e)
        e.openDay(3)
        e.openCopy()
        verify(!findChild(e, "copyDay2").checked)
        verify(!findChild(e, "copyDay3").enabled)
        findChild(e, "copyCancel").trigger()
        e.home()

        //The cool-off switch goes back to the server's value after a refused write.
        e.kind = "cool"
        e.openDay(1)
        var off = findChild(e, "coolOffButton")
        off.trigger()
        verify(off.checked)
        answerAll(function(c) { return c.id === "coff" ? "failed" : "" })
        verify(!off.checked)
        off.trigger()
        answerAll()
        verify(off.checked)
        off.trigger()
        answerAll()
        verify(!off.checked)
    }
}
