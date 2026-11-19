pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Slot form: start, end, setpoint, days. Times step by 15 min, any minute
//typed is accepted.
ColumnLayout {
    id: root

    property string fontFamily
    property bool simpleMode: false
    property bool readOnly: false
    //An existing segment is edited: Delete and the level priority are shown.
    property bool editing: false
    property int level: 0
    //Highest level that has slots: a segment cannot go below it.
    property int levelCount: 0
    property string message: ""

    property int start: 18 * 60
    property int end: 22 * 60
    property real setpoint: 20
    property var days: []

    signal saveRequested(var edit)
    signal deleteRequested()
    signal cancelRequested()
    signal moveLevelRequested(int delta)

    spacing: Units.dp(10)

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

    function degrees(v) {
        return Number(v).toFixed(1).replace(".", Qt.locale().decimalPoint) + " °C"
    }

    function open(values) {
        start = values.start
        end = values.end
        setpoint = TR.clampSetpoint(values.setpoint)
        days = values.days.slice()
        editing = !!values.editing
        level = values.level || 0
        message = ""
        startField.text = TR.hhmm(start)
        endField.text = TR.hhmm(end)
    }

    function step(minutes, delta) {
        var m = Math.round(minutes / TR.TimeStep) * TR.TimeStep
        if (m === minutes || (delta > 0) === (m > minutes))
            m = minutes + delta
        return Math.max(0, Math.min(TR.DayMinutes, m))
    }

    function toggleDay(d) {
        var list = days.slice()
        var i = list.indexOf(d)
        if (i >= 0)
            list.splice(i, 1)
        else
            list.push(d)
        list.sort()
        days = list
    }

    function submit() {
        var s = TR.parseHhmm(startField.text), e = TR.parseHhmm(endField.text)
        if (s < 0 || e < 0) {
            message = qsTr("Enter the time as 17:30")
            return
        }
        if (s === e) {
            message = qsTr("The end must follow the start")
            return
        }
        if (!days.length) {
            message = qsTr("Choose at least one day")
            return
        }
        start = s
        end = e
        message = ""
        saveRequested({ start: s, end: e, setpoint: setpoint, days: days.slice() })
    }

    component TimeField: RowLayout {
        id: tf
        property alias text: field.text
        property alias field: field
        property string label
        property string fontFamily
        property string placeholder
        property bool readOnly
        signal stepped(int delta)

        spacing: Units.dp(6)
        Text {
            text: tf.label
            color: Theme.whiteColor
            font { family: tf.fontFamily; pixelSize: Units.dp(15) }
            Layout.preferredWidth: Units.dp(80)
        }
        TimeRangeButton {
            fontFamily: tf.fontFamily
            text: "−"
            enabled: !tf.readOnly
            Accessible.name: qsTr("%1 minus 15 min").arg(tf.label)
            onClicked: tf.stepped(-TR.TimeStep)
        }
        Controls.TextField {
            id: field
            enabled: !tf.readOnly
            placeholderText: tf.placeholder
            placeholderTextColor: Theme.mutedColor
            color: Theme.whiteColor
            font { family: tf.fontFamily; pixelSize: Units.dp(16) }
            horizontalAlignment: Text.AlignHCenter
            inputMethodHints: Qt.ImhPreferNumbers
            Layout.preferredWidth: Units.dp(90)
            Layout.preferredHeight: Units.dp(44)
            Accessible.name: tf.label
            background: Rectangle {
                color: Theme.colorAlpha(Theme.whiteColor, 0.06)
                border.color: field.activeFocus ? Theme.blueColor : Theme.borderColor
                radius: Units.dp(4)
            }
        }
        TimeRangeButton {
            fontFamily: tf.fontFamily
            text: "+"
            enabled: !tf.readOnly
            Accessible.name: qsTr("%1 plus 15 min").arg(tf.label)
            onClicked: tf.stepped(TR.TimeStep)
        }
    }

    TimeField {
        id: startField
        objectName: "startField"
        label: qsTr("Start")
        placeholder: "08:00"
        fontFamily: root.fontFamily
        readOnly: root.readOnly
        onStepped: (delta) => {
            var m = TR.parseHhmm(text)
            root.start = root.step(m < 0 ? root.start : m, delta)
            text = TR.hhmm(root.start)
        }
    }

    TimeField {
        id: endField
        objectName: "endField"
        label: qsTr("End")
        placeholder: "17:30"
        fontFamily: root.fontFamily
        readOnly: root.readOnly
        onStepped: (delta) => {
            var m = TR.parseHhmm(text)
            root.end = root.step(m < 0 ? root.end : m, delta)
            text = TR.hhmm(root.end)
        }
    }

    RowLayout {
        spacing: Units.dp(6)
        Text {
            text: qsTr("Setpoint")
            color: Theme.whiteColor
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
            Layout.preferredWidth: Units.dp(80)
        }
        TimeRangeButton {
            fontFamily: root.fontFamily
            text: "−"
            enabled: !root.readOnly && root.setpoint > TR.SetpointMin
            Accessible.name: qsTr("Lower the setpoint")
            onClicked: root.setpoint = TR.clampSetpoint(root.setpoint - TR.SetpointStep)
        }
        Text {
            objectName: "setpointText"
            text: root.degrees(root.setpoint)
            color: Theme.whiteColor
            font { family: root.fontFamily; pixelSize: Units.dp(18) }
            horizontalAlignment: Text.AlignHCenter
            Layout.preferredWidth: Units.dp(90)
        }
        TimeRangeButton {
            fontFamily: root.fontFamily
            text: "+"
            enabled: !root.readOnly && root.setpoint < TR.SetpointMax
            Accessible.name: qsTr("Raise the setpoint")
            onClicked: root.setpoint = TR.clampSetpoint(root.setpoint + TR.SetpointStep)
        }
    }

    Text {
        text: qsTr("Days")
        color: Theme.whiteColor
        font { family: root.fontFamily; pixelSize: Units.dp(15) }
    }

    Flow {
        spacing: Units.dp(4)
        Layout.fillWidth: true
        Repeater {
            model: 7
            delegate: TimeRangeButton {
                required property int index
                objectName: "formDay" + (index + 1)
                fontFamily: root.fontFamily
                text: root.dayShort(index + 1)
                sidePadding: Units.dp(8)
                Accessible.name: root.dayName(index + 1)
                checkable: true
                checked: root.days.indexOf(index + 1) >= 0
                enabled: !root.readOnly
                onClicked: root.toggleDay(index + 1)
            }
        }
    }

    RowLayout {
        visible: root.editing && !root.simpleMode && root.level > 0
        spacing: Units.dp(6)
        Text {
            objectName: "levelText"
            text: qsTr("Priority %1").arg(root.level)
            color: Theme.whiteColor
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
        }
        TimeRangeButton {
            objectName: "levelUp"
            fontFamily: root.fontFamily
            icon: "chevronU"
            Accessible.name: qsTr("Raise the priority")
            enabled: !root.readOnly && root.level > 1
            onClicked: root.moveLevelRequested(-1)
        }
        TimeRangeButton {
            objectName: "levelDown"
            fontFamily: root.fontFamily
            icon: "chevronD"
            Accessible.name: qsTr("Lower the priority")
            enabled: !root.readOnly && root.level < root.levelCount
            onClicked: root.moveLevelRequested(1)
        }
    }

    Text {
        objectName: "formMessage"
        visible: root.message !== ""
        text: root.message
        color: Theme.redColor
        wrapMode: Text.WordWrap
        font { family: root.fontFamily; pixelSize: Units.dp(14) }
        Layout.fillWidth: true
        Accessible.role: Accessible.AlertMessage
    }

    Flow {
        spacing: Units.dp(6)
        Layout.fillWidth: true
        TimeRangeButton {
            objectName: "deleteButton"
            visible: root.editing && !root.simpleMode
            fontFamily: root.fontFamily
            danger: true
            text: qsTr("Delete")
            enabled: !root.readOnly
            onClicked: root.deleteRequested()
        }
        TimeRangeButton {
            objectName: "cancelButton"
            visible: !root.simpleMode
            fontFamily: root.fontFamily
            text: qsTr("Cancel")
            onClicked: root.cancelRequested()
        }
        TimeRangeButton {
            objectName: "saveButton"
            fontFamily: root.fontFamily
            accent: true
            text: root.simpleMode ? qsTr("Apply") : qsTr("Save")
            enabled: !root.readOnly
            onClicked: root.submit()
        }
    }
}
