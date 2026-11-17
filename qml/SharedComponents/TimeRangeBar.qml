pragma ComponentBehavior: Bound
import QtQuick
import "."
import "TimeRangeLogic.js" as TR

//24 h bar of one day: the segments of the levels, the default in the gaps.
//The level number and the setpoint are written: the colour never carries the
//information alone.
Item {
    id: root

    property string fontFamily
    property string kind: "heat"
    property var levels: []
    property int day: 1
    property var defaultSetpoint: null
    property bool defaultOff: false
    //Week preview: 28 px, no text, a click selects the day.
    property bool compact: false
    property bool showAxis: !compact
    property bool showNow: false
    property int nowMinutes: 0
    //Desktop and kiosk: a drag on the bar selects a slot (D2).
    property bool dragEnabled: false
    property bool readOnly: false
    //Pending selection of a drag, minutes; -1 when none.
    property int selStart: -1
    property int selEnd: -1

    signal segmentActivated(var segment)
    signal gapActivated(int minute)
    signal rangeSelected(int start, int end)
    signal dayActivated()

    readonly property var segments: TR.fullDay(levels, day, defaultSetpoint, defaultOff)
    readonly property real barHeight: compact ? Units.dp(28) : Units.dp(44)

    implicitHeight: barHeight + (showAxis ? Units.dp(20) : 0)

    function levelColor(n) {
        var heat = ["#ffe08a", "#ffc247", "#ff9a3c", "#f2693a", "#d94a4a", "#b3345f"]
        var cool = ["#a6e3f2", "#6cc9e6", "#38b0d3", "#3b8fd0", "#4a6fd0", "#6a55c4"]
        if (n < 1 || n > 6)
            return Theme.colorAlpha(Theme.whiteColor, 0.08)
        return (kind === "cool" ? cool : heat)[n - 1]
    }

    function degrees(v) {
        if (v === null || v === undefined)
            return "--"
        var text = Number(v).toFixed(1).replace(/\.0$/, "")
        return text.replace(".", Qt.locale().decimalPoint) + "°"
    }

    function segmentText(seg, narrow) {
        if (seg.level === 0)
            return seg.off ? qsTr("Off") : (narrow ? "" : degrees(seg.setpoint))
        return narrow ? "N" + seg.level : "N" + seg.level + " " + degrees(seg.setpoint)
    }

    function accessibleText(seg) {
        var value = seg.off ? qsTr("off") : degrees(seg.setpoint)
        var from = TR.hhmm(seg.start), to = TR.hhmm(seg.end)
        if (seg.level === 0)
            return qsTr("Default, %1 to %2, %3").arg(from).arg(to).arg(value)
        return qsTr("Level %1, %2 to %3, %4").arg(seg.level).arg(from).arg(to).arg(value)
    }

    Rectangle {
        id: track
        width: parent.width
        height: root.barHeight
        radius: Units.dp(4)
        color: Theme.colorAlpha(Theme.whiteColor, 0.05)
        border.color: Theme.borderColor
        clip: true

        Repeater {
            model: root.segments

            delegate: Rectangle {
                id: seg
                required property var modelData
                readonly property real ratio: (modelData.end - modelData.start) / TR.DayMinutes

                objectName: "segment_" + modelData.level + "_" + modelData.start
                x: track.width * modelData.start / TR.DayMinutes
                width: Math.max(1, track.width * ratio)
                height: track.height
                color: modelData.level === 0 ? "transparent" : root.levelColor(modelData.level)
                border.color: activeFocus ? Theme.whiteColor : "transparent"
                border.width: Units.dp(2)
                activeFocusOnTab: !root.compact && modelData.level > 0

                Accessible.role: Accessible.Button
                Accessible.name: root.accessibleText(modelData)
                Keys.onReturnPressed: root.activate(modelData)
                Keys.onEnterPressed: root.activate(modelData)

                Text {
                    anchors.fill: parent
                    anchors.margins: Units.dp(2)
                    visible: !root.compact
                    text: root.segmentText(seg.modelData, seg.ratio < 0.09)
                    color: seg.modelData.level === 0 ? Theme.mutedColor
                         : seg.modelData.level <= 3 ? "#111111" : "#ffffff"
                    font { family: root.fontFamily; pixelSize: Units.dp(13) }
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
        }

        Rectangle {
            objectName: "selection"
            visible: root.selStart >= 0 && root.selEnd > root.selStart
            x: track.width * root.selStart / TR.DayMinutes
            width: track.width * (root.selEnd - root.selStart) / TR.DayMinutes
            height: track.height
            color: Theme.colorAlpha(Theme.blueColor, 0.35)
            border.color: Theme.blueColor
        }

        Rectangle {
            visible: root.showNow
            x: track.width * root.nowMinutes / TR.DayMinutes - width / 2
            width: Units.dp(2)
            height: track.height
            color: Theme.whiteColor
            Accessible.ignored: true
        }

        MouseArea {
            id: input
            anchors.fill: parent
            enabled: !root.readOnly || root.compact
            property int pressMinute: -1
            property bool dragging: false

            onPressed: (mouse) => {
                pressMinute = TR.snap(mouse.x, width)
                dragging = false
            }
            onPositionChanged: (mouse) => {
                if (!root.dragEnabled || root.compact || pressMinute < 0)
                    return
                var m = TR.snap(mouse.x, width)
                if (!dragging && Math.abs(mouse.x - width * pressMinute / TR.DayMinutes) < Units.dp(8))
                    return
                dragging = true
                root.selStart = Math.min(pressMinute, m)
                root.selEnd = Math.max(pressMinute, m)
            }
            onReleased: (mouse) => {
                if (root.compact) {
                    root.dayActivated()
                } else if (dragging) {
                    //A drag selects at least 15 min.
                    if (root.selEnd - root.selStart < TR.TimeStep)
                        root.selEnd = Math.min(TR.DayMinutes, root.selStart + TR.TimeStep)
                    root.rangeSelected(root.selStart, root.selEnd)
                } else {
                    root.activateAt(Math.floor(mouse.x / width * TR.DayMinutes))
                }
                pressMinute = -1
                dragging = false
            }
            onCanceled: {
                pressMinute = -1
                dragging = false
            }
        }
    }

    function activate(seg) {
        if (seg.level > 0)
            segmentActivated(seg)
        else
            gapActivated(seg.start)
    }

    function activateAt(minute) {
        for (var i = 0; i < segments.length; i++)
            if (minute >= segments[i].start && minute < segments[i].end)
                return activate(segments[i])
    }

    Repeater {
        model: root.showAxis ? [0, 6, 12, 18, 24] : []

        delegate: Text {
            required property int modelData
            y: root.barHeight + Units.dp(2)
            x: Math.max(0, Math.min(root.width - width, root.width * modelData / 24 - width / 2))
            text: String(modelData)
            color: Theme.mutedColor
            font { family: root.fontFamily; pixelSize: Units.dp(12) }
            Accessible.ignored: true
        }
    }
}
