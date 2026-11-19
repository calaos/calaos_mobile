pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."
import "ClimateLogic.js" as Logic

//Summary of the time program of a climate: the switch, the level that is
//active, the next change and, in auto, the range. The slots are edited
//elsewhere (TimeRangeEditor); this card only writes program_enabled.
ColumnLayout {
    id: root

    property var climate
    property string fontFamily
    property var client: typeof programClient !== "undefined" ? programClient : null
    property bool readOnly: false
    property int pixelSize: Units.dp(15)

    signal editRequested()

    readonly property var program: climate && climate.devState ? Logic.programState(climate.devState) : null
    readonly property bool present: climate && climate.caps.program !== null && program !== null
    readonly property var ios: climate && climate.modelData && climate.modelData.device && climate.modelData.device.ios
                               ? climate.modelData.device.ios : ({})
    readonly property string enableId: ios.program_enabled ? String(ios.program_enabled) : ""
    readonly property bool offline: !climate || climate.status.kind === "offline"
    readonly property bool canToggle: present && enableId !== "" && client !== null && !readOnly && !offline

    //What the user asked for, until the state says the same or the write fails.
    property var requested: undefined
    readonly property bool programOn: requested !== undefined ? requested : (program ? program.enabled : false)
    readonly property string modeSet: program ? program.modeSet : ""

    property real now: climate ? climate.clock() : Date.now()

    visible: present
    spacing: Units.dp(8)

    onProgramChanged: {
        //A measurement landing between the click and the confirmation must not flip the switch back
        if (program && program.enabled === requested)
            requested = undefined
        if (climate)
            now = climate.clock()
    }

    function toggle() {
        if (!canToggle)
            return
        var want = !programOn
        requested = want
        pendingWrite = client.setState(enableId, want ? "true" : "false")
        settle.restart()
    }

    property string pendingWrite: ""

    Connections {
        target: root.client
        ignoreUnknownSignals: true

        function onFinished(requestId, ok) {
            if (requestId === root.pendingWrite && !ok)
                root.requested = undefined
        }
    }

    //A write that was accepted but never shown by the state must not leave the switch lying
    Timer {
        id: settle
        interval: 10000
        onTriggered: root.requested = undefined
    }

    Timer {
        interval: 30000
        repeat: true
        running: root.present && root.program !== null && root.program.next !== null
        onTriggered: root.now = root.climate.clock()
    }

    function degrees(v) {
        return Logic.formatDegrees(v, Qt.locale().decimalPoint, 1)
    }

    function setName(k) {
        return k === "cool" ? qsTr("Cooling") : qsTr("Heating")
    }

    function levelText(k, set) {
        if (!set)
            return ""
        if (k === "cool" && set.setpoint === null)
            return qsTr("%1: stopped").arg(setName(k))
        var sp = set.setpoint !== null ? degrees(set.setpoint) : ""
        return set.activeLevel > 0
            ? qsTr("%1: level %2, %3").arg(setName(k)).arg(set.activeLevel).arg(sp)
            : qsTr("%1: default, %2").arg(setName(k)).arg(sp)
    }

    readonly property var activeLines: {
        if (!program || modeSet === "")
            return []
        var out = []
        var sets = modeSet === "auto" ? ["heat", "cool"] : [modeSet]
        for (var i = 0; i < sets.length; i++) {
            var t = levelText(sets[i], program[sets[i]])
            if (t !== "")
                out.push(t)
        }
        return out
    }

    readonly property string nextText: {
        if (!program || !programOn || modeSet === "" || requested !== undefined)
            return ""
        var n = program.next
        if (n === null)
            return qsTr("No change in the next 7 days")
        var when = new Date(n.at)
        var sameDay = when.toDateString() === new Date(now).toDateString()
        var time = sameDay ? Qt.formatTime(when, "HH:mm") : Qt.formatDateTime(when, "ddd HH:mm")
        var what = n.level > 0 ? qsTr("%1 level %2").arg(setName(n.set)).arg(n.level)
                               : qsTr("%1 default").arg(setName(n.set))
        if (n.setpoint !== null)
            what = qsTr("%1 at %2").arg(what).arg(degrees(n.setpoint))
        else if (n.set === "cool")
            what = qsTr("%1 stopped").arg(setName(n.set))
        var left = Logic.untilChange(n.at, now)
        var delay = left.days > 0 ? qsTr("in %1 d %2 h").arg(left.days).arg(left.hours)
                  : left.hours > 0 ? qsTr("in %1 h %2 min").arg(left.hours).arg(left.minutes)
                  : qsTr("in %1 min").arg(left.minutes)
        return qsTr("%1, %2 (%3)").arg(time).arg(what).arg(delay)
    }

    readonly property bool showRange: modeSet === "auto" && program !== null && program.heat !== null
                                      && program.cool !== null && program.heat.setpoint !== null
                                      && program.cool.setpoint !== null
    property bool helpOpen: false

    RowLayout {
        Layout.fillWidth: true
        spacing: Units.dp(12)

        Text {
            Layout.fillWidth: true
            color: Theme.whiteColor
            font { family: root.fontFamily; pixelSize: root.pixelSize + Units.dp(1) }
            text: qsTr("Program enabled")
        }

        Rectangle {
            id: track
            objectName: "programSwitch"
            readonly property bool on: root.programOn
            Layout.preferredWidth: Units.dp(56)
            Layout.preferredHeight: Units.dp(44)
            radius: Units.dp(22)
            //The 44 dp target is the item; the visible track is thinner
            color: "transparent"
            opacity: root.canToggle ? 1 : 0.4
            activeFocusOnTab: root.canToggle

            Accessible.role: Accessible.CheckBox
            Accessible.name: qsTr("Program enabled")
            Accessible.checkable: true
            Accessible.checked: on
            Accessible.onToggleAction: root.toggle()
            Keys.onSpacePressed: root.toggle()
            Keys.onReturnPressed: root.toggle()

            Rectangle {
                anchors.centerIn: parent
                width: Units.dp(48)
                height: Units.dp(28)
                radius: height / 2
                color: track.on ? Theme.blueColor : Theme.colorAlpha(Theme.whiteColor, 0.15)
                border.color: track.activeFocus ? Theme.whiteColor : "transparent"
                border.width: Units.dp(2)

                Rectangle {
                    y: Units.dp(3)
                    x: track.on ? parent.width - width - Units.dp(3) : Units.dp(3)
                    width: Units.dp(22)
                    height: width
                    radius: width / 2
                    color: Theme.whiteColor
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.toggle()
            }
        }
    }

    Text {
        objectName: "programPaused"
        Layout.fillWidth: true
        visible: root.present && !root.programOn
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: root.fontFamily; pixelSize: root.pixelSize }
        text: qsTr("Program paused")
    }

    Text {
        objectName: "programNoSet"
        Layout.fillWidth: true
        visible: root.present && root.programOn && root.modeSet === ""
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: root.fontFamily; pixelSize: root.pixelSize }
        text: qsTr("No program for this mode")
    }

    Repeater {
        model: root.activeLines

        Text {
            required property string modelData
            objectName: "programLevel"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.programOn ? Theme.whiteColor : Theme.mutedColor
            font { family: root.fontFamily; pixelSize: root.pixelSize }
            text: modelData
        }
    }

    RowLayout {
        objectName: "programRange"
        Layout.fillWidth: true
        visible: root.showRange
        spacing: Units.dp(8)

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.whiteColor
            font { family: root.fontFamily; pixelSize: root.pixelSize }
            text: root.showRange
                  ? qsTr("Range %1 – %2").arg(root.degrees(root.program.heat.setpoint)).arg(root.degrees(root.program.cool.setpoint))
                  : ""
        }

        Item {
            objectName: "programHelpButton"
            Layout.preferredWidth: Units.dp(44)
            Layout.preferredHeight: Units.dp(44)
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: root.helpOpen ? qsTr("Hide range help") : qsTr("About the range")
            Accessible.checkable: true
            Accessible.checked: root.helpOpen
            Accessible.onPressAction: root.helpOpen = !root.helpOpen
            Accessible.onToggleAction: root.helpOpen = !root.helpOpen
            Keys.onSpacePressed: root.helpOpen = !root.helpOpen
            Keys.onReturnPressed: root.helpOpen = !root.helpOpen

            ClimateIcon {
                anchors.centerIn: parent
                name: "info"
                size: Units.dp(22)
                color: root.helpOpen ? Theme.blueColor : Theme.mutedColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.helpOpen = !root.helpOpen
            }
        }
    }

    Text {
        objectName: "programHelp"
        Layout.fillWidth: true
        visible: root.showRange && root.helpOpen
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: root.fontFamily; pixelSize: root.pixelSize - Units.dp(1) }
        text: root.climate && root.climate.caps.bounds && root.climate.caps.bounds.minRange > 0
              ? qsTr("In auto, the program sets a range: the lower end follows the heating, the upper end the cooling. The two are kept at least %1 apart.")
                    .arg(root.degrees(root.climate.caps.bounds.minRange))
              : qsTr("In auto, the program sets a range: the lower end follows the heating, the upper end the cooling. The two are kept a minimum apart.")
    }

    Connections {
        target: root.climate ? root.climate.modelData : null
        ignoreUnknownSignals: true
        function onDeviceChange() { strips.refreshStates() }
    }

    ClimateProgramStrips {
        id: strips
        objectName: "programStrips"
        Layout.fillWidth: true
        active: root.present && root.modeSet === "auto"
        client: root.client
        device: root.climate && root.climate.modelData ? root.climate.modelData.device : null
        fontFamily: root.fontFamily
        now: root.now
    }

    Text {
        objectName: "programNext"
        Layout.fillWidth: true
        visible: root.nextText !== ""
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: root.fontFamily; pixelSize: root.pixelSize }
        text: root.nextText
    }

    MouseArea {
        objectName: "programEdit"
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Program and settings")
        Accessible.onPressAction: root.editRequested()
        Keys.onReturnPressed: root.editRequested()
        Keys.onSpacePressed: root.editRequested()
        Layout.fillWidth: true
        Layout.preferredHeight: Units.dp(50)
        visible: root.present
        onClicked: root.editRequested()

        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: Qt.rgba(1, 1, 1, 0.1)
        }

        Text {
            anchors { left: parent.left; leftMargin: Units.dp(12); verticalCenter: parent.verticalCenter }
            color: Theme.mutedColor
            font { family: root.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
            text: qsTr("Program and settings")
        }

        ClimateIcon {
            anchors { right: parent.right; rightMargin: Units.dp(4); verticalCenter: parent.verticalCenter }
            name: "chevronR"
            size: Units.dp(20)
            color: Theme.mutedColor
        }
    }
}
