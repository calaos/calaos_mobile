pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Program of a climate, one page at a time: Week, then Day, then the period
//form. The ids come from device.ios, the writes go through programClient.
//simpleMode is the one-period form reused by the scenarios: it never writes
//program_enabled nor the defaults.
FocusScope {
    id: root

    property string fontFamily
    property var device: null
    property var client: typeof programClient !== "undefined" ? programClient : null
    property bool simpleMode: false
    //Different temperatures one direction may use (capabilities.program.max_levels); 0: what the device exposes.
    property int maxLevels: 0
    property bool phone: false
    property bool readOnly: false
    property string kind: "heat"
    property int day: TR.isoDay(new Date())
    //Name of the thermostat, and the setpoint it keeps while the program is paused.
    property string title
    property var holdSetpoint
    readonly property real rowHeight: Units.dp(phone ? 48 : 56)
    //Touch targets: 44 on a phone, 56 on the wall kiosk
    readonly property real targetSize: Units.dp(phone ? 44 : 56)

    signal applied()
    //Close on the Week page: the host closes its dialog or sheet.
    signal closeRequested()

    readonly property var ios: device && device.ios ? device.ios : ({})
    readonly property var kinds: {
        var out = []
        if (TR.levelsFromIos(ios, "heat").length)
            out.push("heat")
        if (TR.levelsFromIos(ios, "cool").length)
            out.push("cool")
        return out
    }
    property var levelsByKind: ({ heat: [], cool: [] })
    property var states: ({})
    readonly property var levels: levelsByKind[kind] || []
    readonly property bool offline: client === null || sessionLost
    //Writes replace the whole list of a level: nothing may be written before the
    //levels have been read, nor after a read failed, or the server's slots are lost.
    //A failed write stays locked until Retry replays it (a fresh save would be computed
    //on half-written levels) or the form is cancelled.
    readonly property bool locked: readOnly || offline || scopeDenied || busy || loading || readFailed
                                   || retryAction !== null || pendingWrite !== null
    readonly property bool loading: Object.keys(reads).length > 0
    readonly property bool busy: writeId !== "" || queue.length > 0
    property bool loaded: false
    property bool readFailed: false
    //A failed read must not replace the failed write: only a replay or Cancel clears it.
    property var pendingWrite: null
    property bool sessionLost: false
    //The account may read the program but not write it: sticky until the client changes.
    property bool scopeDenied: false
    property string banner: ""
    property var retryAction: null
    property int nowMinutes: 0

    property var reads: ({})
    property var queue: []
    property string writeId: ""
    property var writeDone: null
    //The segment being edited: { day, start, end, level } or null for a new one.
    property var editing: null
    readonly property string page: stack.currentItem ? stack.currentItem.page : ""
    readonly property bool formOpen: page === "form"
    readonly property bool copyOpen: copySheet.opened
    readonly property var currentForm: formOpen ? stack.currentItem.form : null

    function kindName(k) {
        return k === "cool" ? qsTr("Cooling") : qsTr("Heating")
    }

    function stateValue(role) {
        var id = ios[role]
        return id && states.hasOwnProperty(id) ? states[id] : undefined
    }

    readonly property var defaultSetpoint: TR.toSetpoint(stateValue(kind + "_default"))
    readonly property bool coolOff: kind === "cool" && String(stateValue("cool_default_off")) === "true"
    readonly property bool hasPause: !simpleMode && ios["program_enabled"] !== undefined
    readonly property bool paused: hasPause && String(stateValue("program_enabled")) === "false"

    function dayName(d) {
        switch (d) {
        case 1: return qsTr("Monday")
        case 2: return qsTr("Tuesday")
        case 3: return qsTr("Wednesday")
        case 4: return qsTr("Thursday")
        case 5: return qsTr("Friday")
        case 6: return qsTr("Saturday")
        default: return qsTr("Sunday")
        }
    }

    function dayShort(d) {
        switch (d) {
        case 1: return qsTr("Mon", "Monday, short")
        case 2: return qsTr("Tue", "Tuesday, short")
        case 3: return qsTr("Wed", "Wednesday, short")
        case 4: return qsTr("Thu", "Thursday, short")
        case 5: return qsTr("Fri", "Friday, short")
        case 6: return qsTr("Sat", "Saturday, short")
        default: return qsTr("Sun", "Sunday, short")
        }
    }

    function periodCount(d) {
        return TR.daySegments(levels, d).length
    }

    function countText(n) {
        return n === 0 ? qsTr("no period") : n === 1 ? qsTr("1 period") : qsTr("%1 periods").arg(n)
    }

    function degrees(v) {
        return v === null || v === undefined ? "--"
             : Number(v).toFixed(1).replace(/\.0$/, "").replace(".", Qt.locale().decimalPoint) + "°"
    }

    // --- reads ------------------------------------------------------------

    function track(id, what) {
        if (id === "") {
            sessionLost = true
            return
        }
        var r = Object.assign({}, reads)
        r[id] = what
        reads = r
    }

    function reload() {
        if (!client || !completed)
            return
        sessionLost = false
        readFailed = false
        var next = { heat: TR.levelsFromIos(ios, "heat"), cool: TR.levelsFromIos(ios, "cool") }
        //Keeps the slots on screen until the new ones are read.
        for (var k in next)
            for (var i = 0; i < next[k].length; i++) {
                var old = TR.levelById(levelsByKind[k] || [], next[k][i].ioId)
                if (old) {
                    next[k][i].slots = old.slots
                    next[k][i].setpoint = old.setpoint
                }
            }
        levelsByKind = next
        if (kinds.length && kinds.indexOf(kind) < 0)
            kind = kinds[0]
        for (k in next)
            for (i = 0; i < next[k].length; i++)
                readLevel(k, next[k][i].ioId)
        var ids = stateIds()
        if (ids.length)
            track(client.getStates(ids), { states: true })
    }

    function stateIds() {
        var ids = TR.stateIds(ios, "heat").concat(TR.stateIds(ios, "cool"))
        if (hasPause)
            ids.push(ios["program_enabled"])
        return ids
    }

    //io_changed of the climate: the setpoints may have moved, the slots have not
    function refreshStates() {
        if (!client || !completed || busy || loading)
            return
        var ids = stateIds()
        if (ids.length)
            track(client.getStates(ids), { states: true })
    }

    function readLevel(k, ioId) {
        track(client.getTimeRange(ioId), { kind: k, ioId: ioId })
    }

    function writing(ioId) {
        for (var i = 0; i < queue.length; i++)
            if (queue[i].ioId === ioId)
                return true
        return false
    }

    function onRead(what, ok, answer) {
        if (!ok) {
            readFailed = true
            if (pendingWrite === null) {
                banner = qsTr("Could not read the program, try again")
                retryAction = function() { root.reload() }
            }
            return
        }
        var next = Object.assign({}, levelsByKind)
        if (what.states) {
            states = answer
            next.heat = TR.withSetpoints(next.heat, answer)
            next.cool = TR.withSetpoints(next.cool, answer)
        } else {
            var l = TR.levelById(next[what.kind], what.ioId)
            //The optimistic state of a level being written wins until timerange_changed.
            if (!l || writing(what.ioId))
                return
            next[what.kind] = TR.withSlots(next[what.kind], l.n, TR.fromRanges(answer.ranges))
        }
        levelsByKind = next
    }

    // --- writes -----------------------------------------------------------

    function errorText(error) {
        if (error === "invalid ranges" || error === "invalid range")
            return qsTr("The end must follow the start")
        if (error === "scope denied")
            return qsTr("This account cannot change the program")
        return qsTr("Not saved, try again")
    }

    //Setpoints first, then one set_timerange per changed level, in sequence.
    //A retry resends this very result (idempotent writes): recomputing it from the
    //reloaded levels would not undo a half-applied swap. `restore` puts kind and day back.
    function commit(res, restore, failText, onSaved) {
        if (locked)
            return
        var ops = []
        var i
        for (i = 0; i < res.setpoints.length; i++) {
            var l = res.levels[TR.levelIndex(res.levels, res.setpoints[i].n)]
            if (l.setpointId)
                ops.push({ stateId: l.setpointId, value: String(res.setpoints[i].setpoint) })
        }
        for (i = 0; i < res.changed.length; i++) {
            var c = res.levels[TR.levelIndex(res.levels, res.changed[i])]
            ops.push({ ioId: c.ioId, ranges: TR.toRanges(c.slots) })
        }
        var next = Object.assign({}, levelsByKind)
        next[kind] = res.levels
        levelsByKind = next
        banner = ""
        retryAction = null
        var again = function() { if (restore) restore(); root.commit(res, restore, failText, onSaved) }
        if (!failText && ops.length > 1)
            failText = qsTr("Incomplete save: not saved, try again")
        writeDone = { action: again, failText: failText || "", saved: onSaved || null }
        queue = ops
        sendNext()
    }

    function writeState(stateId, value, action) {
        if (locked)
            return
        var s = Object.assign({}, states)
        s[stateId] = value
        states = s
        banner = ""
        writeDone = { action: action, failText: "", saved: null }
        queue = [{ stateId: stateId, value: value }]
        sendNext()
    }

    function sendNext() {
        if (!queue.length) {
            writeId = ""
            var done = writeDone
            writeDone = null
            if (done && done.saved)
                done.saved()
            if (simpleMode)
                applied()
            return
        }
        var op = queue[0]
        var id = op.stateId ? client.setState(op.stateId, op.value)
                            : client.setTimeRange(op.ioId, op.ranges)
        if (id === "") {
            writeFailed("transport")
            return
        }
        writeId = id
    }

    function writeFailed(error) {
        var done = writeDone
        writeId = ""
        queue = []
        writeDone = null
        banner = done && done.failText !== "" && error !== "scope denied" ? done.failText : errorText(error)
        scopeDenied = scopeDenied || error === "scope denied"
        pendingWrite = done && !scopeDenied ? done.action : null
        retryAction = pendingWrite
        //Shows what the server really has.
        reload()
    }

    function retry() {
        var action = retryAction
        banner = ""
        retryAction = null
        //The replayed write carries its own levels, it needs no successful read first.
        if (pendingWrite !== null) {
            pendingWrite = null
            readFailed = false
        }
        if (action)
            action()
    }

    Connections {
        target: root.client
        ignoreUnknownSignals: true

        function onFinished(requestId, ok, answer, error) {
            if (root.reads.hasOwnProperty(requestId)) {
                var what = root.reads[requestId]
                var r = Object.assign({}, root.reads)
                delete r[requestId]
                root.reads = r
                root.onRead(what, ok, answer)
                if (!root.loading)
                    root.loaded = true
                return
            }
            if (requestId !== root.writeId || requestId === "")
                return
            if (!ok) {
                root.writeFailed(error)
                return
            }
            root.queue = root.queue.slice(1)
            root.writeId = ""
            root.sendNext()
        }

        function onTimeRangeChanged(ioId) {
            for (var k in root.levelsByKind)
                if (TR.levelById(root.levelsByKind[k], ioId) && !root.writing(ioId))
                    root.readLevel(k, ioId)
        }
    }

    //The initial properties fire their change handlers too: one read at completion.
    property bool completed: false
    //A new device object arrives at every io_changed: only other ids mean other levels
    readonly property string iosKey: TR.iosKey(ios)
    onIosKeyChanged: reload()
    onClientChanged: {
        scopeDenied = false
        reload()
    }
    Component.onCompleted: {
        completed = true
        reload()
    }
    onSimpleModeChanged: {
        if (completed)
            stack.replace(null, simpleMode ? formPage : weekPage, {}, StackView.Immediate)
    }

    // --- actions ----------------------------------------------------------

    // --- navigation -------------------------------------------------------

    function openDay(d) {
        if (page !== "week")
            return
        day = d
        stack.push(dayPage)
    }

    function pushForm(values) {
        if (formOpen || simpleMode)
            return
        var item = stack.push(formPage, { edit: values.editing === true })
        item.form.open(values)
    }

    //A new period starts on the temperature most used this week in this direction.
    function newSetpoint() {
        var v = TR.mostUsedSetpoint(levels, levelLimit)
        return v !== null ? v : (defaultSetpoint !== null ? defaultSetpoint : 20)
    }

    function openNew(start, end) {
        editing = null
        pushForm({ start: start, end: end,
                   setpoint: newSetpoint() })
    }

    function openSegment(seg) {
        editing = { day: day, start: seg.start, end: seg.end, level: seg.level }
        pushForm({ start: seg.start, end: seg.end, setpoint: seg.setpoint,
                   editing: true, level: seg.level })
    }

    function openCopy() {
        if (page !== "day" || locked)
            return
        copySheet.open(day)
    }

    function closeForm() {
        editing = null
        if (pendingWrite !== null) {
            pendingWrite = null
            retryAction = null
            banner = ""
            if (readFailed)
                reload()
        } else if (!readFailed && retryAction !== null) {
            retryAction = null
            banner = ""
        }
        if (formOpen && !simpleMode)
            stack.pop()
    }

    //Named back button and Back / Escape: false on the Week page, which the host closes.
    function back() {
        if (copyOpen) {
            copySheet.close()
            return true
        }
        if (simpleMode || stack.depth <= 1)
            return false
        if (formOpen)
            closeForm()
        else
            stack.pop()
        return true
    }

    function goBack() {
        if (!back())
            closeRequested()
    }

    //The host reopens the editor on the week.
    function home() {
        editing = null
        if (!simpleMode && stack.depth > 1)
            stack.pop(null, StackView.Immediate)
    }

    function setPaused(p) {
        if (hasPause)
            writeState(ios["program_enabled"], p ? "false" : "true", function() { root.setPaused(p) })
    }

    Keys.onPressed: (event) => {
        if ((event.key === Qt.Key_Back || event.key === Qt.Key_Escape) && (copyOpen || stack.depth > 1) && !simpleMode)
            event.accepted = true
    }
    //On release, as the hosts do: the release must not reach the page below.
    Keys.onReleased: (event) => {
        if ((event.key === Qt.Key_Back || event.key === Qt.Key_Escape) && back())
            event.accepted = true
    }

    function savePeriod(edit) {
        applyRequest({ days: edit.days, start: edit.start, end: edit.end, setpoint: edit.setpoint,
                       original: edit.original, swapWith: edit.swapWith })
    }

    readonly property int levelLimit: maxLevels > 0 ? Math.min(maxLevels, Math.max(levels.length, 1)) : levels.length
    readonly property string maxLevelsText: qsTr("This week already uses %1 different temperatures. Choose one of them:").arg(levelLimit)

    //The view stays open until the server has the change: a failure keeps the input.
    function applyRequest(request) {
        var res = TR.applySlot(levels, request)
        if (!res.ok) {
            var text = res.error === "max_levels" ? maxLevelsText
                                                  : qsTr("The end must follow the start")
            if (currentForm)
                currentForm.message = text
            else
                banner = text
            return
        }
        var k = kind
        commit(res, function() { root.kind = k }, "", function() { root.closeForm() })
    }

    function deleteSlot() {
        if (!editing)
            return
        var e = editing
        var k = kind
        commit(TR.deleteSegment(levels, e.day, e.start, e.end),
               function() { root.kind = k }, "", function() { root.closeForm() })
    }

    function copyTo(targets) {
        var res = TR.copyDay(levels, day, targets)
        if (!res.ok) {
            if (res.error === "empty_source")
                banner = qsTr("Nothing to copy: this day has no period")
            return
        }
        var k = kind, src = day
        copySheet.close()
        commit(res, function() { root.kind = k; root.day = src },
               qsTr("Incomplete copy: not saved, try again"))
    }

    function stepDefault(delta) {
        var id = ios[kind + "_default"]
        var v = TR.clampSetpoint((defaultSetpoint !== null ? defaultSetpoint : 20) + delta)
        if (id)
            writeState(id, String(v), null)
    }

    function setCoolOff(off) {
        var id = ios["cool_default_off"]
        if (id)
            writeState(id, off ? "true" : "false", null)
    }

    Timer {
        interval: 60000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: {
            var d = new Date()
            root.nowMinutes = d.getHours() * 60 + d.getMinutes()
        }
    }

    // --- view -------------------------------------------------------------

    readonly property string backText: page === "week" ? qsTr("Close")
                                     : page === "day" ? qsTr("Week") : dayName(day)
    readonly property string headerTitle: page === "week" ? qsTr("Schedule")
                                        : page === "day" ? dayName(day)
                                        : stack.currentItem && stack.currentItem.edit ? qsTr("Period") : qsTr("New period")
    readonly property string headerSubtitle: {
        if (page === "week")
            return title
        var parts = title !== "" ? [title] : []
        parts.push(kindName(kind))
        if (page === "day")
            parts.push(countText(periodCount(day)))
        return parts.join(" · ")
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Units.dp(10)

        Item {
            objectName: "programHeader"
            visible: !root.simpleMode
            Layout.fillWidth: true
            implicitHeight: headerColumn.implicitHeight + Units.dp(10)

            ColumnLayout {
                id: headerColumn
                anchors { left: parent.left; right: parent.right; top: parent.top }
                spacing: Units.dp(8)

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Units.dp(12)

                    TimeRangeButton {
                        objectName: "backButton"
                        fontFamily: root.fontFamily
                        implicitHeight: root.rowHeight
                        sidePadding: Units.dp(20)
                        leading: root.page === "week" ? "close" : "chevronL"
                        text: root.backText
                        textColor: Theme.blueColor
                        onClicked: root.goBack()
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Units.dp(2)
                        Text {
                            objectName: "headerTitle"
                            Layout.fillWidth: true
                            text: root.headerTitle
                            color: Theme.whiteColor
                            elide: Text.ElideRight
                            font { family: root.fontFamily; pixelSize: Units.dp(20); weight: Font.DemiBold }
                            Accessible.role: Accessible.Heading
                            Accessible.name: text
                        }
                        Text {
                            objectName: "headerSubtitle"
                            Layout.fillWidth: true
                            visible: text !== ""
                            text: root.headerSubtitle
                            color: Theme.mutedColor
                            wrapMode: Text.WordWrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            font { family: root.fontFamily; pixelSize: Units.dp(13) }
                        }
                    }

                    Item {
                        id: wideTabs
                        implicitWidth: tabs.implicitWidth
                        implicitHeight: tabs.implicitHeight
                        visible: !root.phone && tabs.shown
                    }

                    TimeRangeButton {
                        objectName: "copyButton"
                        visible: root.page === "day"
                        fontFamily: root.fontFamily
                        implicitHeight: root.rowHeight
                        sidePadding: Units.dp(20)
                        leading: "copy"
                        text: qsTr("Copy to…")
                        enabled: !root.locked && root.periodCount(root.day) > 0
                        onClicked: root.openCopy()
                    }
                }

                Item {
                    id: phoneTabs
                    Layout.fillWidth: true
                    implicitHeight: tabs.implicitHeight
                    visible: root.phone && tabs.shown
                }
            }

            Row {
                id: tabs
                readonly property bool shown: root.page === "week" && root.kinds.length > 1
                parent: root.phone ? phoneTabs : wideTabs
                width: root.phone ? parent.width : implicitWidth
                spacing: 0

                Repeater {
                    model: root.kinds
                    delegate: TimeRangeButton {
                        required property string modelData
                        objectName: "tab_" + modelData
                        fontFamily: root.fontFamily
                        implicitHeight: root.rowHeight
                        width: root.phone ? tabs.width / root.kinds.length : implicitWidth
                        sidePadding: Units.dp(36)
                        text: root.kindName(modelData)
                        accent: root.kind === modelData
                        accentColor: modelData === "cool" ? Theme.blueColor : Theme.yellowColor
                        checkable: true
                        Accessible.role: Accessible.PageTab
                        Accessible.checked: root.kind === modelData
                        onClicked: root.kind = modelData
                    }
                }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Theme.borderColor
            }
        }

        Text {
            objectName: "offlineText"
            visible: !root.simpleMode && (root.offline || root.readOnly)
            Layout.fillWidth: true
            text: qsTr("Offline: the schedule is read-only")
            color: Theme.amberColor
            wrapMode: Text.WordWrap
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }

        Rectangle {
            objectName: "banner"
            visible: root.banner !== ""
            color: Theme.colorAlpha(Theme.redColor, 0.2)
            border.color: Theme.redColor
            radius: Units.dp(4)
            Layout.fillWidth: true
            implicitHeight: bannerRow.implicitHeight + Units.dp(12)
            Accessible.role: Accessible.AlertMessage
            Accessible.name: root.banner

            RowLayout {
                id: bannerRow
                anchors.fill: parent
                anchors.margins: Units.dp(6)
                Text {
                    text: root.banner
                    color: Theme.whiteColor
                    wrapMode: Text.WordWrap
                    font { family: root.fontFamily; pixelSize: Units.dp(14) }
                    Layout.fillWidth: true
                }
                TimeRangeButton {
                    objectName: "retryButton"
                    visible: root.retryAction !== null
                    fontFamily: root.fontFamily
                    text: qsTr("Retry")
                    enabled: !root.offline && !root.busy && !root.loading
                    onClicked: root.retry()
                }
            }
        }

        StackView {
            id: stack
            objectName: "programStack"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            initialItem: root.simpleMode ? formPage : weekPage

            readonly property int duration: Theme.reduceMotion ? 0 : 150
            pushEnter: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: stack.duration }
                NumberAnimation { property: "x"; from: Units.dp(40); to: 0; duration: stack.duration; easing.type: Easing.OutCubic }
            }
            pushExit: Transition {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: stack.duration }
            }
            popEnter: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: stack.duration }
            }
            popExit: Transition {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: stack.duration }
                NumberAnimation { property: "x"; from: 0; to: Units.dp(40); duration: stack.duration; easing.type: Easing.InCubic }
            }
        }
    }

    Component {
        id: weekPage
        TimeRangeWeek {
            editor: root
        }
    }

    Component {
        id: dayPage
        TimeRangeDay {
            editor: root
        }
    }

    Component {
        id: formPage
        Flickable {
            id: formFlick
            readonly property string page: "form"
            property bool edit: false
            property alias form: pageForm
            contentHeight: pageForm.implicitHeight + Units.dp(16)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            TimeRangePeriod {
                id: pageForm
                objectName: "periodView"
                editor: root
                width: formFlick.width
                fontFamily: root.fontFamily
                readOnly: root.locked
                onSaveRequested: (edit) => root.savePeriod(edit)
                onDeleteRequested: root.deleteSlot()
                onCancelRequested: root.closeForm()
                Component.onCompleted: {
                    if (root.simpleMode)
                        open({ start: 18 * 60, end: 22 * 60, setpoint: root.newSetpoint() })
                }
            }
        }
    }

    TimeRangeCopy {
        id: copySheet
        anchors.fill: parent
        z: 10
        fontFamily: root.fontFamily
        levels: root.levels
        names: root
        onCopyRequested: (targets) => root.copyTo(targets)
        onCancelRequested: close()
    }
}
