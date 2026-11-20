pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Period view: start, end and temperature by steppers. Priority between
//overlapping periods is the only place a level number is shown.
ColumnLayout {
    id: root

    property var editor
    property string fontFamily
    property bool readOnly: false
    //An existing period is edited: Delete is offered.
    property bool editing: false
    property var original: null
    property string message: ""

    property int start: 18 * 60
    property int end: 22 * 60
    property real setpoint: 20
    //Overlap draft: the other level takes over this period's priority.
    property bool swapped: false
    property bool confirming: false
    //Levels as they were when the view opened: saving rewrites the live ones.
    property var base: []

    signal saveRequested(var edit)
    signal deleteRequested()
    signal cancelRequested()

    readonly property bool simple: editor ? editor.simpleMode : false
    readonly property bool crossesMidnight: end <= start
    readonly property var plain: preview(0)
    readonly property int otherLevel: plain.overlaps.length ? plain.overlaps[0].level : 0
    readonly property var shown: swapped && otherLevel > 0 ? preview(otherLevel) : plain
    readonly property var overlap: shown.overlaps.length ? shown.overlaps[0] : null
    readonly property bool valid: start !== end && !plain.tooMany
    readonly property string rangeText: original ? TR.hhmm(original.start) + " – " + TR.hhmm(original.end) : ""

    spacing: Units.dp(12)

    //The scenario form opens before the program is read: follow it until the first edit.
    Connections {
        target: root.editor
        function onLevelsChanged() {
            if (root.simple && !root.editor.busy)
                root.base = root.editor.levels
        }
    }

    function preview(swapWith) {
        return TR.previewOverlap(base, request(swapWith))
    }

    function request(swapWith) {
        return { days: [editor.day], start: start, end: end, setpoint: setpoint,
                 original: original, swapWith: swapWith, maxLevels: editor.levelLimit }
    }

    function open(values) {
        start = values.start
        end = values.end
        setpoint = TR.clampSetpoint(values.setpoint)
        editing = !!values.editing
        original = editing ? { day: editor.day, start: values.start, end: values.end, level: values.level } : null
        base = editor.levels
        swapped = false
        confirming = false
        message = ""
    }

    function stepTime(minutes, delta, low, high) {
        var m = Math.round(minutes / TR.TimeStep) * TR.TimeStep
        if (m === minutes || (delta > 0) === (m > minutes))
            m = minutes + delta
        return Math.max(low, Math.min(high, m))
    }

    function submit() {
        if (!valid || readOnly)
            return
        var edit = request(swapped ? otherLevel : 0)
        edit.original = original
        saveRequested(edit)
    }

    function timeRange(a, b) {
        return TR.hhmm(a) + " – " + TR.hhmm(b)
    }

    component Banner: Rectangle {
        id: banner
        property color tone: Theme.blueColor
        property alias text: bannerText.text
        property alias actionText: action.text
        signal actionClicked()
        Layout.fillWidth: true
        implicitHeight: bannerRow.implicitHeight + Units.dp(16)
        radius: Units.dp(4)
        color: Theme.colorAlpha(tone, 0.16)
        border.color: tone
        Accessible.role: Accessible.AlertMessage
        Accessible.name: bannerText.text

        RowLayout {
            id: bannerRow
            anchors { fill: parent; margins: Units.dp(8) }
            spacing: Units.dp(10)
            Text {
                id: bannerText
                Layout.fillWidth: true
                color: Theme.whiteColor
                wrapMode: Text.WordWrap
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
            }
            TimeRangeButton {
                id: action
                visible: text !== ""
                fontFamily: root.fontFamily
                implicitHeight: root.editor.targetSize
                enabled: !root.readOnly
                onClicked: banner.actionClicked()
            }
        }
    }

    component Stepper: Rectangle {
        id: st
        property string label
        property string valueText
        property string hint
        property bool canDown: true
        property bool canUp: true
        property string downName
        property string upName
        signal down()
        signal up()
        Layout.fillWidth: true
        implicitHeight: root.editor.rowHeight + Units.dp(8)
        radius: Units.dp(6)
        color: Theme.colorAlpha(Theme.whiteColor, 0.05)
        border.color: Theme.borderColor

        RowLayout {
            anchors { fill: parent; leftMargin: Units.dp(14); rightMargin: Units.dp(6) }
            spacing: Units.dp(6)
            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredWidth: 0
                Layout.minimumWidth: 0
                spacing: 0
                Text {
                    text: st.label
                    color: Theme.mutedColor
                    font { family: root.fontFamily; pixelSize: Units.dp(13) }
                }
                Text {
                    visible: st.hint !== ""
                    text: st.hint
                    color: Theme.amberColor
                    font { family: root.fontFamily; pixelSize: Units.dp(13); weight: Font.DemiBold }
                }
            }
            TimeRangeButton {
                objectName: st.objectName + "Down"
                fontFamily: root.fontFamily
                implicitHeight: root.editor.targetSize
                implicitWidth: root.editor.targetSize
                text: "−"
                textSize: Units.dp(28)
                textColor: Theme.blueColor
                enabled: !root.readOnly && st.canDown
                Accessible.name: st.downName
                onClicked: st.down()
            }
            Text {
                objectName: st.objectName + "Value"
                Layout.preferredWidth: Units.dp(76)
                horizontalAlignment: Text.AlignHCenter
                text: st.valueText
                color: Theme.whiteColor
                font { family: root.fontFamily; pixelSize: Units.dp(22); weight: Font.DemiBold }
                Accessible.role: Accessible.StaticText
                Accessible.name: st.label + " " + text
            }
            TimeRangeButton {
                objectName: st.objectName + "Up"
                fontFamily: root.fontFamily
                implicitHeight: root.editor.targetSize
                implicitWidth: root.editor.targetSize
                text: "+"
                textSize: Units.dp(28)
                textColor: Theme.blueColor
                enabled: !root.readOnly && st.canUp
                Accessible.name: st.upName
                onClicked: st.up()
            }
        }
    }

    Stepper {
        objectName: "startStepper"
        label: qsTr("Start")
        valueText: TR.hhmm(root.start)
        canDown: root.start > 0
        canUp: root.start < TR.DayMinutes - TR.TimeStep
        downName: qsTr("Start earlier")
        upName: qsTr("Start later")
        onDown: root.start = root.stepTime(root.start, -TR.TimeStep, 0, TR.DayMinutes - TR.TimeStep)
        onUp: root.start = root.stepTime(root.start, TR.TimeStep, 0, TR.DayMinutes - TR.TimeStep)
    }

    Stepper {
        objectName: "endStepper"
        label: qsTr("End")
        valueText: TR.hhmm(root.end)
        hint: root.crossesMidnight ? qsTr("+1 day") : ""
        canDown: root.end > TR.TimeStep
        canUp: root.end < TR.DayMinutes
        downName: qsTr("End earlier")
        upName: qsTr("End later")
        onDown: root.end = root.stepTime(root.end, -TR.TimeStep, TR.TimeStep, TR.DayMinutes)
        onUp: root.end = root.stepTime(root.end, TR.TimeStep, TR.TimeStep, TR.DayMinutes)
    }

    Stepper {
        objectName: "setpointStepper"
        label: qsTr("Temperature")
        valueText: root.editor.degrees(root.setpoint)
        canDown: root.setpoint > TR.clampSetpoint(-1000)
        canUp: root.setpoint < TR.clampSetpoint(1000)
        downName: qsTr("Lower the temperature")
        upName: qsTr("Raise the temperature")
        onDown: root.setpoint = TR.clampSetpoint(root.setpoint - TR.SetpointStep)
        onUp: root.setpoint = TR.clampSetpoint(root.setpoint + TR.SetpointStep)
    }

    Banner {
        objectName: "midnightBanner"
        visible: root.crossesMidnight && root.start !== root.end
        text: qsTr("This period crosses midnight. It will be saved as two periods: %1 and %2.")
              .arg(root.editor.dayName(root.editor.day) + " " + root.timeRange(root.start, TR.DayMinutes))
              .arg(root.editor.dayName(TR.nextDay(root.editor.day)) + " " + root.timeRange(0, root.end))
    }

    Banner {
        objectName: "overlapBanner"
        tone: Theme.amberColor
        visible: root.overlap !== null && root.valid
        text: {
            var o = root.overlap
            if (!o)
                return ""
            var head = qsTr("Overlaps “%1” (%2).").arg(root.timeRange(o.range.start, o.range.end)).arg(root.editor.degrees(o.setpoint))
            var body = o.wins
                ? qsTr("Between %1 and %2, this period (%3) applies: it takes priority.")
                  .arg(TR.hhmm(o.from)).arg(TR.hhmm(o.to)).arg(root.editor.degrees(root.setpoint))
                : qsTr("Between %1 and %2, that period (%3) applies: it takes priority.")
                  .arg(TR.hhmm(o.from)).arg(TR.hhmm(o.to)).arg(root.editor.degrees(o.setpoint))
            var mine = root.shown.level
            return head + " " + body + " " + qsTr("Level %1 comes before level %2.")
                   .arg(Math.min(mine, o.level)).arg(Math.max(mine, o.level))
        }
        actionText: qsTr("Swap")
        onActionClicked: root.swapped = !root.swapped
    }

    Banner {
        objectName: "tooManyBanner"
        tone: Theme.amberColor
        visible: root.plain.tooMany
        text: root.editor.maxLevelsText
    }

    Flow {
        objectName: "usedSetpoints"
        visible: root.plain.tooMany
        Layout.fillWidth: true
        spacing: Units.dp(8)
        Repeater {
            model: root.plain.used
            TimeRangeButton {
                required property real modelData
                objectName: "usedSetpoint"
                fontFamily: root.fontFamily
                implicitHeight: root.editor.targetSize
                text: root.editor.degrees(modelData)
                Accessible.name: qsTr("Set to %1").arg(text)
                enabled: !root.readOnly
                onClicked: root.setpoint = TR.clampSetpoint(modelData)
            }
        }
    }

    Text {
        objectName: "formMessage"
        visible: root.message !== ""
        text: root.message
        color: Theme.redColor
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        font { family: root.fontFamily; pixelSize: Units.dp(14) }
    }

    Text {
        objectName: "deleteConfirm"
        visible: root.confirming
        Layout.fillWidth: true
        text: qsTr("Delete the period %1?").arg(root.rangeText)
        color: Theme.whiteColor
        wrapMode: Text.WordWrap
        font { family: root.fontFamily; pixelSize: Units.dp(17); weight: Font.DemiBold }
        Accessible.role: Accessible.AlertMessage
        Accessible.name: text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Units.dp(10)

        TimeRangeButton {
            objectName: "deleteButton"
            visible: root.editing && !root.simple && !root.confirming
            fontFamily: root.fontFamily
            implicitHeight: root.editor.targetSize
            text: qsTr("Delete")
            danger: true
            enabled: !root.readOnly
            onClicked: root.confirming = true
        }
        Item { Layout.fillWidth: true }
        TimeRangeButton {
            objectName: "cancelButton"
            visible: !root.simple
            fontFamily: root.fontFamily
            implicitHeight: root.editor.targetSize
            text: qsTr("Cancel")
            onClicked: root.confirming ? root.confirming = false : root.cancelRequested()
        }
        TimeRangeButton {
            objectName: "confirmDeleteButton"
            visible: root.confirming
            fontFamily: root.fontFamily
            implicitHeight: root.editor.targetSize
            text: qsTr("Delete")
            danger: true
            enabled: !root.readOnly
            onClicked: root.deleteRequested()
        }
        TimeRangeButton {
            objectName: "saveButton"
            visible: !root.confirming
            fontFamily: root.fontFamily
            implicitHeight: root.editor.targetSize
            text: root.simple ? qsTr("Apply") : qsTr("Save")
            accent: true
            enabled: root.valid && !root.readOnly
            onClicked: root.submit()
        }
    }
}
