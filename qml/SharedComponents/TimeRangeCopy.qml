pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//"Copy this day": each target day becomes the source day. Asks first only
//when a target already has slots.
ColumnLayout {
    id: root

    property string fontFamily
    property var levels: []
    property int source: 1
    property var targets: []
    //The form: it holds the translated day names.
    property var names: null
    property var replaced: []

    readonly property bool sourceEmpty: TR.slotCount(levels, source) === 0

    signal copyRequested(var targets)
    signal cancelRequested()

    spacing: Units.dp(8)

    function dayName(d) {
        return names ? names.dayName(d) : String(d)
    }

    function dayShort(d) {
        return names ? names.dayShort(d) : String(d)
    }

    function open(day) {
        source = day
        targets = []
        replaced = []
    }

    function setTargets(days) {
        targets = days.filter(function(d) { return d !== root.source })
        replaced = []
    }

    function toggle(d) {
        var list = targets.slice()
        var i = list.indexOf(d)
        if (i >= 0)
            list.splice(i, 1)
        else
            list.push(d)
        list.sort()
        setTargets(list)
    }

    function confirmText() {
        var parts = []
        for (var i = 0; i < replaced.length; i++) {
            var name = dayName(replaced[i].day)
            parts.push(replaced[i].count === 1 ? qsTr("%1 (1 slot)").arg(name)
                                               : qsTr("%1 (%2 slots)").arg(name).arg(replaced[i].count))
        }
        var list = parts.length > 1
                ? qsTr("%1 and %2").arg(parts.slice(0, -1).join(", ")).arg(parts[parts.length - 1])
                : parts[0]
        return qsTr("%1 will be replaced by those of %2.").arg(list).arg(dayName(source))
    }

    function copy() {
        if (!targets.length || sourceEmpty)
            return
        var r = TR.replacedDays(levels, source, targets)
        if (r.length && !replaced.length) {
            replaced = r
            return
        }
        replaced = []
        copyRequested(targets.slice())
    }

    Text {
        text: qsTr("Copy %1 to").arg(root.dayName(root.source))
        color: Theme.whiteColor
        font { family: root.fontFamily; pixelSize: Units.dp(15) }
    }

    Text {
        objectName: "copyEmpty"
        visible: root.sourceEmpty
        text: qsTr("Nothing to copy: this day has no slot")
        color: Theme.amberColor
        wrapMode: Text.WordWrap
        font { family: root.fontFamily; pixelSize: Units.dp(14) }
        Layout.fillWidth: true
    }

    Flow {
        spacing: Units.dp(4)
        Layout.fillWidth: true
        Repeater {
            model: 7
            delegate: TimeRangeButton {
                required property int index
                objectName: "copyDay" + (index + 1)
                fontFamily: root.fontFamily
                text: root.dayShort(index + 1)
                sidePadding: Units.dp(8)
                Accessible.name: root.dayName(index + 1)
                checkable: true
                checked: root.targets.indexOf(index + 1) >= 0
                enabled: index + 1 !== root.source
                onClicked: root.toggle(index + 1)
            }
        }
    }

    Flow {
        spacing: Units.dp(4)
        Layout.fillWidth: true
        TimeRangeButton {
            objectName: "copyWeekdays"
            fontFamily: root.fontFamily
            text: qsTr("Weekdays")
            onClicked: root.setTargets([1, 2, 3, 4, 5])
        }
        TimeRangeButton {
            objectName: "copyWeekend"
            fontFamily: root.fontFamily
            text: qsTr("Weekend")
            onClicked: root.setTargets([6, 7])
        }
        TimeRangeButton {
            objectName: "copyAll"
            fontFamily: root.fontFamily
            text: qsTr("All")
            onClicked: root.setTargets([1, 2, 3, 4, 5, 6, 7])
        }
    }

    Rectangle {
        objectName: "copyConfirm"
        visible: root.replaced.length > 0
        color: Theme.colorAlpha(Theme.amberColor, 0.15)
        border.color: Theme.amberColor
        radius: Units.dp(4)
        Layout.fillWidth: true
        implicitHeight: confirmColumn.implicitHeight + Units.dp(16)
        Accessible.role: Accessible.AlertMessage
        Accessible.name: confirmLabel.text

        ColumnLayout {
            id: confirmColumn
            anchors.fill: parent
            anchors.margins: Units.dp(8)
            Text {
                id: confirmLabel
                objectName: "copyConfirmText"
                text: root.replaced.length ? root.confirmText() : ""
                color: Theme.whiteColor
                wrapMode: Text.WordWrap
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
                Layout.fillWidth: true
            }
            Flow {
                spacing: Units.dp(6)
                Layout.fillWidth: true
                TimeRangeButton {
                    fontFamily: root.fontFamily
                    text: qsTr("Cancel")
                    onClicked: root.replaced = []
                }
                TimeRangeButton {
                    objectName: "copyReplace"
                    fontFamily: root.fontFamily
                    accent: true
                    text: qsTr("Replace and copy")
                    onClicked: root.copy()
                }
            }
        }
    }

    Flow {
        visible: root.replaced.length === 0
        spacing: Units.dp(6)
        Layout.fillWidth: true
        TimeRangeButton {
            fontFamily: root.fontFamily
            text: qsTr("Cancel")
            onClicked: root.cancelRequested()
        }
        TimeRangeButton {
            objectName: "copyGo"
            fontFamily: root.fontFamily
            accent: true
            text: qsTr("Copy")
            enabled: root.targets.length > 0 && !root.sourceEmpty
            onClicked: root.copy()
        }
    }
}
