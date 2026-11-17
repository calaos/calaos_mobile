pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Program of a climate: the slots of its heat and cool levels, one day at a
//time (D1). The ids come from device.ios, the writes go through
//programClient. simpleMode is the one-slot form reused by the scenarios: it
//never writes program_enabled nor the defaults.
Item {
    id: root

    property string fontFamily
    property var device: null
    property var client: typeof programClient !== "undefined" ? programClient : null
    property bool simpleMode: false
    //Phone: the form is a full screen sheet and there is no drag (D2).
    property bool phone: false
    property bool readOnly: false
    property string kind: "heat"
    property int day: TR.isoDay(new Date())

    signal applied()

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
    readonly property bool locked: readOnly || offline || busy
    readonly property bool loading: Object.keys(reads).length > 0
    readonly property bool busy: writeId !== "" || queue.length > 0
    property bool loaded: false
    property bool sessionLost: false
    property string banner: ""
    property var retryAction: null
    property int nowMinutes: 0

    property var reads: ({})
    property var queue: []
    property string writeId: ""
    property var writeDone: null
    //The segment being edited: { day, start, end, level } or null for a new one.
    property var editing: null
    property bool formOpen: simpleMode
    property bool copyOpen: false

    function kindName(k) {
        return k === "cool" ? qsTr("Cooling") : qsTr("Heating")
    }

    function stateValue(role) {
        var id = ios[role]
        return id && states.hasOwnProperty(id) ? states[id] : undefined
    }

    readonly property var defaultSetpoint: TR.toSetpoint(stateValue(kind + "_default"))
    readonly property bool coolOff: kind === "cool" && String(stateValue("cool_default_off")) === "true"

    function degrees(v) {
        return v === null || v === undefined ? "--"
             : Number(v).toFixed(1).replace(/\.0$/, "").replace(".", Qt.locale().decimalPoint) + " °C"
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
        var ids = TR.stateIds(ios, "heat").concat(TR.stateIds(ios, "cool"))
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
        if (!ok)
            return
        var next = Object.assign({}, levelsByKind)
        if (what.states) {
            states = answer
            next.heat = TR.withSetpoints(next.heat, answer)
            next.cool = TR.withSetpoints(next.cool, answer)
        } else {
            var l = TR.levelById(next[what.kind], what.ioId)
            if (!l)
                return
            next[what.kind] = TR.withSlots(next[what.kind], l.n, TR.fromRanges(answer.ranges))
        }
        levelsByKind = next
    }

    // --- writes -----------------------------------------------------------

    function errorText(error) {
        if (error === "config_not_written")
            return qsTr("Not saved, try again")
        if (error === "invalid ranges" || error === "invalid range")
            return qsTr("The end must follow the start")
        if (error === "scope denied")
            return qsTr("This account cannot change the program")
        return qsTr("Saving failed, try again")
    }

    //Setpoints first, then one set_timerange per changed level, in sequence.
    function commit(res, action, failText) {
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
        writeDone = { action: action, failText: failText || "" }
        queue = ops
        sendNext()
    }

    function writeState(stateId, value, action) {
        var s = Object.assign({}, states)
        s[stateId] = value
        states = s
        banner = ""
        writeDone = { action: action, failText: "" }
        queue = [{ stateId: stateId, value: value }]
        sendNext()
    }

    function sendNext() {
        if (!queue.length) {
            writeId = ""
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
        banner = done && done.failText !== "" ? done.failText : errorText(error)
        retryAction = done ? done.action : null
        //Shows what the server really has.
        reload()
    }

    function retry() {
        var action = retryAction
        banner = ""
        retryAction = null
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
    onDeviceChanged: reload()
    onClientChanged: reload()
    Component.onCompleted: {
        completed = true
        reload()
    }

    // --- actions ----------------------------------------------------------

    function openNew(start, end) {
        editing = null
        form.open({ start: start, end: end, days: [day],
                    setpoint: defaultSetpoint !== null ? defaultSetpoint : 20 })
        formOpen = true
        copyOpen = false
    }

    function openSegment(seg) {
        editing = { day: day, start: seg.start, end: seg.end, level: seg.level }
        form.open({ start: seg.start, end: seg.end, days: [day], setpoint: seg.setpoint,
                    editing: true, level: seg.level })
        formOpen = true
        copyOpen = false
    }

    function closeForm() {
        bar.selStart = -1
        bar.selEnd = -1
        editing = null
        formOpen = simpleMode
    }

    function saveSlot(edit) {
        applyRequest({ days: edit.days, start: edit.start, end: edit.end, setpoint: edit.setpoint,
                       original: editing ? { day: editing.day, start: editing.start, end: editing.end } : null })
    }

    function applyRequest(request) {
        var res = TR.applySlot(levels, request)
        if (!res.ok) {
            var text = res.error === "max_levels" ? qsTr("6 levels maximum for %1").arg(kindName(kind))
                                                  : qsTr("The end must follow the start")
            if (formOpen)
                form.message = text
            else
                banner = text
            return
        }
        var k = kind
        closeForm()
        commit(res, function() { root.kind = k; root.applyRequest(request) })
    }

    function deleteSlot() {
        if (!editing)
            return
        var e = editing
        closeForm()
        commit(TR.deleteSegment(levels, e.day, e.start, e.end), null)
    }

    function moveLevel(delta) {
        if (!editing)
            return
        var a = editing.level, b = editing.level + delta
        var res = TR.swapLevels(levels, a, b)
        if (!res.ok)
            return
        closeForm()
        commit(res, null)
    }

    function copyTo(targets) {
        var res = TR.copyDay(levels, day, targets)
        if (!res.ok) {
            if (res.error === "empty_source")
                banner = qsTr("Nothing to copy: this day has no slot")
            return
        }
        var k = kind, src = day
        copyOpen = false
        commit(res, function() { root.kind = k; root.day = src; root.copyTo(targets) },
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

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: column.implicitHeight + Units.dp(16)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: !(root.formOpen && root.phone)

        ColumnLayout {
            id: column
            width: flick.width
            spacing: Units.dp(10)

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
                        enabled: !root.offline && !root.busy
                        onClicked: root.retry()
                    }
                }
            }

            Text {
                objectName: "loadingText"
                visible: root.loading && !root.loaded
                text: qsTr("Loading the program…")
                color: Theme.mutedColor
                font { family: root.fontFamily; pixelSize: Units.dp(15) }
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }

            Text {
                objectName: "noProgramText"
                visible: !root.kinds.length
                text: qsTr("This climate has no program.")
                color: Theme.mutedColor
                font { family: root.fontFamily; pixelSize: Units.dp(15) }
            }

            RowLayout {
                visible: root.kinds.length > 1 && !root.simpleMode
                spacing: Units.dp(4)
                Repeater {
                    model: root.kinds
                    delegate: TimeRangeButton {
                        required property string modelData
                        objectName: "tab_" + modelData
                        fontFamily: root.fontFamily
                        text: root.kindName(modelData) + " (" + TR.usedLevels(root.levelsByKind[modelData] || []) + ")"
                        checked: root.kind === modelData
                        Accessible.role: Accessible.PageTab
                        onClicked: {
                            root.kind = modelData
                            root.closeForm()
                            root.copyOpen = false
                        }
                    }
                }
            }

            Flow {
                visible: !root.simpleMode && root.kinds.length > 0
                spacing: Units.dp(4)
                Layout.fillWidth: true
                Repeater {
                    model: 7
                    delegate: TimeRangeButton {
                        required property int index
                        objectName: "day" + (index + 1)
                        fontFamily: root.fontFamily
                        text: form.dayLetter(index + 1)
                        checked: root.day === index + 1
                        Accessible.role: Accessible.PageTab
                        Accessible.name: form.dayName(index + 1)
                        onClicked: {
                            root.day = index + 1
                            root.closeForm()
                            root.copyOpen = false
                        }
                    }
                }
            }

            TimeRangeBar {
                id: bar
                objectName: "dayBar"
                visible: !root.simpleMode && root.kinds.length > 0
                Layout.fillWidth: true
                fontFamily: root.fontFamily
                kind: root.kind
                levels: root.levels
                day: root.day
                defaultSetpoint: root.defaultSetpoint
                defaultOff: root.coolOff
                showNow: root.day === TR.isoDay(new Date())
                nowMinutes: root.nowMinutes
                dragEnabled: !root.phone
                readOnly: root.locked
                onSegmentActivated: (seg) => root.openSegment(seg)
                onGapActivated: (minute) => {
                    var s = Math.floor(minute / 60) * 60
                    root.openNew(s, Math.min(TR.DayMinutes, s + 60))
                }
                onRangeSelected: (start, end) => root.openNew(start, end)
            }

            Text {
                objectName: "emptyText"
                visible: !root.simpleMode && root.kinds.length > 0 && root.loaded
                         && TR.usedLevels(root.levels) === 0
                text: root.kind === "cool"
                      ? (root.coolOff ? qsTr("No slot in cooling. Default: off.")
                                      : qsTr("No slot in cooling. Default: %1.").arg(root.degrees(root.defaultSetpoint)))
                      : qsTr("No slot in heating. Default: %1.").arg(root.degrees(root.defaultSetpoint))
                color: Theme.mutedColor
                wrapMode: Text.WordWrap
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
                Layout.fillWidth: true
            }

            Flow {
                visible: !root.simpleMode && root.kinds.length > 0
                spacing: Units.dp(6)
                Layout.fillWidth: true
                TimeRangeButton {
                    objectName: "addButton"
                    fontFamily: root.fontFamily
                    text: qsTr("+ Slot")
                    enabled: !root.locked
                    onClicked: root.openNew(18 * 60, 22 * 60)
                }
                TimeRangeButton {
                    objectName: "copyButton"
                    fontFamily: root.fontFamily
                    text: qsTr("Copy this day…")
                    enabled: !root.locked
                    onClicked: {
                        root.closeForm()
                        copyPanel.open(root.day)
                        root.copyOpen = true
                    }
                }
                Text {
                    objectName: "levelCount"
                    height: Units.dp(44)
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Levels: %1 of %2").arg(TR.usedLevels(root.levels)).arg(TR.MaxLevels)
                    color: Theme.mutedColor
                    font { family: root.fontFamily; pixelSize: Units.dp(14) }
                }
            }

            TimeRangeCopy {
                id: copyPanel
                visible: root.copyOpen && !root.simpleMode
                Layout.fillWidth: true
                fontFamily: root.fontFamily
                levels: root.levels
                names: form
                onCopyRequested: (targets) => root.copyTo(targets)
                onCancelRequested: root.copyOpen = false
            }

            Item {
                id: inlineSlot
                visible: root.formOpen && !root.phone
                Layout.fillWidth: true
                implicitHeight: visible ? form.implicitHeight : 0
            }

            RowLayout {
                visible: !root.simpleMode && root.kinds.length > 0
                spacing: Units.dp(6)
                TimeRangeButton {
                    objectName: "defaultDown"
                    visible: !root.coolOff
                    fontFamily: root.fontFamily
                    text: "−"
                    enabled: !root.locked && root.defaultSetpoint !== null
                    Accessible.name: qsTr("Lower the default setpoint")
                    onClicked: root.stepDefault(-TR.SetpointStep)
                }
                Text {
                    objectName: "defaultText"
                    text: root.coolOff ? qsTr("Default: off") : qsTr("Default: %1").arg(root.degrees(root.defaultSetpoint))
                    color: Theme.whiteColor
                    font { family: root.fontFamily; pixelSize: Units.dp(15) }
                }
                TimeRangeButton {
                    objectName: "defaultUp"
                    visible: !root.coolOff
                    fontFamily: root.fontFamily
                    text: "+"
                    enabled: !root.locked && root.defaultSetpoint !== null
                    Accessible.name: qsTr("Raise the default setpoint")
                    onClicked: root.stepDefault(TR.SetpointStep)
                }
            }

            TimeRangeButton {
                objectName: "coolOffButton"
                visible: !root.simpleMode && root.kind === "cool" && root.ios["cool_default_off"] !== undefined
                fontFamily: root.fontFamily
                text: qsTr("Cooling off outside slots")
                checkable: true
                checked: root.coolOff
                enabled: !root.locked
                onClicked: root.setCoolOff(!root.coolOff)
            }

            Text {
                visible: !root.simpleMode && root.kinds.length > 0
                text: qsTr("Week")
                color: Theme.mutedColor
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
            }

            Repeater {
                model: !root.simpleMode && root.kinds.length > 0 ? 7 : 0
                delegate: RowLayout {
                    id: weekRow
                    required property int index
                    Layout.fillWidth: true
                    spacing: Units.dp(6)
                    Text {
                        text: form.dayLetter(weekRow.index + 1)
                        color: root.day === weekRow.index + 1 ? Theme.blueColor : Theme.mutedColor
                        font { family: root.fontFamily; pixelSize: Units.dp(13) }
                        Layout.preferredWidth: Units.dp(16)
                    }
                    TimeRangeBar {
                        objectName: "weekBar" + (weekRow.index + 1)
                        compact: true
                        Layout.fillWidth: true
                        kind: root.kind
                        levels: root.levels
                        day: weekRow.index + 1
                        defaultSetpoint: root.defaultSetpoint
                        defaultOff: root.coolOff
                        Accessible.name: form.dayName(weekRow.index + 1)
                        onDayActivated: {
                            root.day = weekRow.index + 1
                            root.closeForm()
                        }
                    }
                }
            }
        }
    }

    //Phone: full screen sheet over the editor.
    Rectangle {
        id: sheet
        anchors.fill: parent
        visible: root.formOpen && root.phone
        color: Theme.backgroundColor
        z: 2
        MouseArea { anchors.fill: parent }
    }

    TimeRangeForm {
        id: form
        objectName: "slotForm"
        parent: root.phone ? sheet : inlineSlot
        visible: root.formOpen
        width: parent ? parent.width - (root.phone ? Units.dp(32) : 0) : 0
        x: root.phone ? Units.dp(16) : 0
        y: root.phone ? Units.dp(16) : 0
        fontFamily: root.fontFamily
        simpleMode: root.simpleMode
        readOnly: root.locked
        levelCount: root.levels.length
        onSaveRequested: (edit) => root.saveSlot(edit)
        onDeleteRequested: root.deleteSlot()
        onCancelRequested: root.closeForm()
        onMoveLevelRequested: (delta) => root.moveLevel(delta)
        Component.onCompleted: {
            if (root.simpleMode)
                open({ start: 18 * 60, end: 22 * 60, days: [root.day], setpoint: 20 })
        }
    }
}
