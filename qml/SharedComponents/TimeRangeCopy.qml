pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Modal sheet "Copy to…": every target day becomes the source day. It lives
//inside the editor so that Back and Escape reach it before the pages below.
FocusScope {
    id: root

    property string fontFamily
    property var levels: []
    property int source: 1
    property var targets: []
    //The editor: it holds the translated day names and the touch target size.
    property var names: null

    readonly property bool sourceEmpty: TR.slotCount(levels, source) === 0
    readonly property bool opened: visible

    signal copyRequested(var targets)
    signal cancelRequested()

    visible: false

    function dayName(d) {
        return names ? names.dayName(d) : String(d)
    }

    function dayShort(d) {
        return names ? names.dayShort(d) : String(d)
    }

    function open(day) {
        source = day
        targets = []
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
    }

    function setTargets(days) {
        targets = days.filter(function(d) { return d !== root.source })
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

    function copy() {
        if (!targets.length || sourceEmpty)
            return
        copyRequested(targets.slice())
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.colorAlpha(Theme.backgroundColor, 0.7)

        MouseArea {
            anchors.fill: parent
            onClicked: root.cancelRequested()
        }
    }

    Rectangle {
        id: card
        objectName: "copySheet"
        readonly property bool phone: root.width < Units.dp(600)
        width: Math.min(root.width - Units.dp(16), Units.dp(520))
        height: Math.min(sheet.implicitHeight + Units.dp(32), root.height - Units.dp(16))
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: phone ? parent.bottom : undefined
        anchors.verticalCenter: phone ? undefined : parent.verticalCenter
        radius: Units.dp(10)
        color: Theme.backgroundColor
        border.color: Theme.borderColor
        Accessible.role: Accessible.Dialog
        Accessible.name: qsTr("Copy %1 to").arg(root.dayName(root.source))

        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: sheet
            anchors { fill: parent; margins: Units.dp(16) }
            spacing: Units.dp(12)

            Text {
                text: qsTr("Copy %1 to").arg(root.dayName(root.source))
                color: Theme.whiteColor
                font { family: root.fontFamily; pixelSize: Units.dp(18); weight: Font.DemiBold }
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            Text {
                objectName: "copyEmpty"
                visible: root.sourceEmpty
                text: qsTr("Nothing to copy: this day has no period")
                color: Theme.amberColor
                wrapMode: Text.WordWrap
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
                Layout.fillWidth: true
            }

            Flow {
                spacing: Units.dp(6)
                Layout.fillWidth: true
                Repeater {
                    model: 7
                    delegate: TimeRangeButton {
                        id: chip
                        required property int index
                        readonly property bool isSource: index + 1 === root.source
                        objectName: "copyDay" + (index + 1)
                        fontFamily: root.fontFamily
                        implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                        implicitWidth: Math.max(implicitHeight, Units.dp(isSource ? 72 : 52))
                        text: isSource ? root.dayShort(index + 1) + " · " + qsTr("source") : root.dayShort(index + 1)
                        sidePadding: Units.dp(8)
                        Accessible.name: isSource ? qsTr("%1, source").arg(root.dayName(index + 1)) : root.dayName(index + 1)
                        checkable: true
                        checked: root.targets.indexOf(index + 1) >= 0
                        enabled: !isSource
                        onClicked: root.toggle(index + 1)
                    }
                }
            }

            Flow {
                spacing: Units.dp(6)
                Layout.fillWidth: true
                TimeRangeButton {
                    objectName: "copyWeekdays"
                    fontFamily: root.fontFamily
                    implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                    text: qsTr("Weekdays")
                    onClicked: root.setTargets([1, 2, 3, 4, 5])
                }
                TimeRangeButton {
                    objectName: "copyWeekend"
                    fontFamily: root.fontFamily
                    implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                    text: qsTr("Weekend")
                    onClicked: root.setTargets([6, 7])
                }
                TimeRangeButton {
                    objectName: "copyAll"
                    fontFamily: root.fontFamily
                    implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                    text: qsTr("All")
                    onClicked: root.setTargets([1, 2, 3, 4, 5, 6, 7])
                }
            }

            Text {
                objectName: "copyWarning"
                text: qsTr("Replaces the existing periods of these days")
                color: Theme.amberColor
                wrapMode: Text.WordWrap
                font { family: root.fontFamily; pixelSize: Units.dp(14) }
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(10)
                Item { Layout.fillWidth: true }
                TimeRangeButton {
                    objectName: "copyCancel"
                    fontFamily: root.fontFamily
                    implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                    text: qsTr("Cancel")
                    onClicked: root.cancelRequested()
                }
                TimeRangeButton {
                    objectName: "copyGo"
                    fontFamily: root.fontFamily
                    implicitHeight: root.names ? root.names.targetSize : Units.dp(44)
                    accent: true
                    text: root.targets.length === 1 ? qsTr("Copy to 1 day") : qsTr("Copy to %1 days").arg(root.targets.length)
                    enabled: root.targets.length > 0 && !root.sourceEmpty
                    onClicked: root.copy()
                }
            }
        }
    }
}
