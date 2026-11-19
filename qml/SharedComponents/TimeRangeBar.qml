pragma ComponentBehavior: Bound
import QtQuick
import "."
import "TimeRangeLogic.js" as TR

//24 h bar of one day, a preview only: the periods are opened from the rows of
//the editor, never from the bar. The colour never carries the information
//alone: the rows and the accessible names give the times and setpoints.
Item {
    id: root

    property string fontFamily
    property string kind: "heat"
    property var levels: []
    property int day: 1
    property var defaultSetpoint: null
    property bool defaultOff: false
    //Card strips: 28 px, no text.
    property bool compact: false
    property bool showAxis: !compact
    property bool showNow: false
    property int nowMinutes: 0
    property real barHeight: compact ? Units.dp(28) : Units.dp(44)

    readonly property var segments: TR.fullDay(levels, day, defaultSetpoint, defaultOff)

    implicitHeight: barHeight + (showAxis ? Units.dp(20) : 0)

    function levelColor(n) {
        return TR.levelColor(kind === "cool" ? Theme.blueColor : Theme.yellowColor, n, Theme.colorAlpha(Theme.whiteColor, 0.28))
    }

    function degrees(v) {
        if (v === null || v === undefined)
            return "--"
        var text = Number(v).toFixed(1).replace(/\.0$/, "")
        return text.replace(".", Qt.locale().decimalPoint) + "°"
    }

    function accessibleText(seg) {
        var value = seg.off ? qsTr("off") : degrees(seg.setpoint)
        var from = TR.hhmm(seg.start), to = TR.hhmm(seg.end)
        if (seg.level === 0)
            return qsTr("Default, %1 to %2, %3").arg(from).arg(to).arg(value)
        return qsTr("%1 to %2, %3").arg(from).arg(to).arg(value)
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

                Accessible.role: Accessible.StaticText
                Accessible.name: root.accessibleText(modelData)

                Text {
                    anchors.fill: parent
                    //Below 46 px a setpoint no longer fits: no label rather than "…"
                    visible: !root.compact && seg.modelData.level > 0 && seg.width >= Units.dp(46)
                    text: root.degrees(seg.modelData.setpoint)
                    color: seg.modelData.level <= 3 ? Theme.backgroundColor : Theme.whiteColor
                    font { family: root.fontFamily; pixelSize: Units.dp(13) }
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    Accessible.ignored: true
                }
            }
        }

        Rectangle {
            visible: root.showNow
            x: track.width * root.nowMinutes / TR.DayMinutes - width / 2
            width: Units.dp(2)
            height: track.height
            color: Theme.whiteColor
            Accessible.ignored: true
        }
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
