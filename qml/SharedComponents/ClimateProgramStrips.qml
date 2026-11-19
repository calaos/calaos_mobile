pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Today of the two sets of the program, one under the other on the same
//24 h axis (auto mode). Read only: the slots are edited in TimeRangeEditor.
ColumnLayout {
    id: root

    property var device: null
    property var client: typeof programClient !== "undefined" ? programClient : null
    property string fontFamily
    property real now: Date.now()
    //Nothing is read while the strips are not shown.
    property bool active: true

    readonly property var ios: device && device.ios ? device.ios : ({})
    property var levelsByKind: ({ heat: [], cool: [] })
    property var states: ({})
    property var reads: ({})
    property bool completed: false
    readonly property int day: TR.isoDay(new Date(now))
    readonly property int nowMinutes: new Date(now).getHours() * 60 + new Date(now).getMinutes()
    readonly property var rangeSpans: levelsByKind.heat.length === 0 || levelsByKind.cool.length === 0 ? [] : TR.rangeSpans(
        TR.fullDay(levelsByKind.heat, day, TR.toSetpoint(stateValue("heat_default")), false),
        TR.fullDay(levelsByKind.cool, day, TR.toSetpoint(stateValue("cool_default")),
                   String(stateValue("cool_default_off")) === "true"))
    readonly property bool loaded: levelsByKind.heat.length + levelsByKind.cool.length > 0

    visible: active && loaded
    spacing: Units.dp(4)

    function track(id, what) {
        if (id === "")
            return
        var r = Object.assign({}, reads)
        r[id] = what
        reads = r
    }

    function reload() {
        if (!client || !completed || !active)
            return
        var next = { heat: TR.levelsFromIos(ios, "heat"), cool: TR.levelsFromIos(ios, "cool") }
        for (var k in next)
            for (var i = 0; i < next[k].length; i++) {
                var old = TR.levelById(levelsByKind[k] || [], next[k][i].ioId)
                if (old) {
                    next[k][i].slots = old.slots
                    next[k][i].setpoint = old.setpoint
                }
            }
        levelsByKind = next
        for (k in next)
            for (i = 0; i < next[k].length; i++)
                track(client.getTimeRange(next[k][i].ioId), { kind: k, ioId: next[k][i].ioId })
        refreshStates()
    }

    //io_changed of the climate: the setpoints may have moved, the slots have not
    function refreshStates() {
        if (!client || !completed || !active)
            return
        var ids = TR.stateIds(ios, "heat").concat(TR.stateIds(ios, "cool"))
        if (ids.length)
            track(client.getStates(ids), { states: true })
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

    function stateValue(role) {
        var id = ios[role]
        return id && states.hasOwnProperty(id) ? states[id] : undefined
    }

    Connections {
        target: root.client
        ignoreUnknownSignals: true

        function onFinished(requestId, ok, answer, error) {
            if (!root.reads.hasOwnProperty(requestId))
                return
            var what = root.reads[requestId]
            var r = Object.assign({}, root.reads)
            delete r[requestId]
            root.reads = r
            root.onRead(what, ok, answer)
        }

        function onTimeRangeChanged(ioId) {
            for (var k in root.levelsByKind)
                if (TR.levelById(root.levelsByKind[k], ioId))
                    root.track(root.client.getTimeRange(ioId), { kind: k, ioId: ioId })
        }
    }

    //A new device object arrives at every io_changed: only other ids mean other levels
    readonly property string iosKey: TR.iosKey(ios)
    onIosKeyChanged: reload()
    onClientChanged: reload()
    onActiveChanged: reload()
    Component.onCompleted: {
        completed = true
        reload()
    }

    Component {
        id: stripRow

        RowLayout {
            id: row
            required property string modelData
            readonly property bool used: root.levelsByKind[modelData].length > 0
            objectName: "programStrip_" + modelData
            visible: used
            Layout.fillWidth: true
            spacing: Units.dp(8)

            Text {
                Layout.preferredWidth: Units.dp(64)
                color: Theme.mutedColor
                font { family: root.fontFamily; pixelSize: Units.dp(13) }
                elide: Text.ElideRight
                text: row.modelData === "cool" ? qsTr("Cooling") : qsTr("Heating")
            }

            TimeRangeBar {
                objectName: "programStripBar_" + row.modelData
                Layout.fillWidth: true
                compact: true
                readOnly: true
                fontFamily: root.fontFamily
                kind: row.modelData
                levels: root.levelsByKind[row.modelData]
                day: root.day
                defaultSetpoint: TR.toSetpoint(root.stateValue(row.modelData + "_default"))
                defaultOff: row.modelData === "cool" && String(root.stateValue("cool_default_off")) === "true"
                showNow: true
                nowMinutes: root.nowMinutes
                Accessible.name: row.modelData === "cool" ? qsTr("Cooling today") : qsTr("Heating today")
            }
        }
    }
    Repeater { model: ["heat"]; delegate: stripRow }

    //Where both sets hold a setpoint, the auto range lives between the two bars
    RowLayout {
        objectName: "programRangeRow"
        Layout.fillWidth: true
        spacing: Units.dp(8)
        visible: root.rangeSpans.length > 0

        Item { Layout.preferredWidth: Units.dp(64) }

        Item {
            id: hatchHost
            Layout.fillWidth: true
            Layout.preferredHeight: Units.dp(10)

            Repeater {
                model: root.rangeSpans

                delegate: Canvas {
                    id: hatch
                    required property var modelData
                    objectName: "programRange_" + modelData.start
                    x: hatchHost.width * modelData.start / TR.DayMinutes
                    width: Math.max(1, hatchHost.width * (modelData.end - modelData.start) / TR.DayMinutes)
                    height: hatchHost.height
                    onWidthChanged: requestPaint()
                    onPaint: {
                        var c = getContext("2d")
                        c.clearRect(0, 0, width, height)
                        c.strokeStyle = Theme.mutedColor
                        c.lineWidth = 1
                        c.beginPath()
                        for (var k = -height; k < width; k += Units.dp(5)) {
                            c.moveTo(k, height)
                            c.lineTo(k + height, 0)
                        }
                        c.stroke()
                    }
                }
            }
        }
    }

    Repeater { model: ["cool"]; delegate: stripRow }
}
