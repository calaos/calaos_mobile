pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Day page of the program editor: a preview bar, then one row per period. The
//rows open the period, "Add a period" is the last row.
Flickable {
    id: root

    readonly property string page: "day"
    property Item editor
    readonly property string fontFamily: editor ? editor.fontFamily : ""
    readonly property var segments: editor ? TR.daySegments(editor.levels, editor.day) : []
    readonly property bool empty: segments.length === 0
    readonly property bool cool: editor ? editor.kind === "cool" : false

    contentHeight: column.implicitHeight + Units.dp(8)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function deg(v) {
        return editor ? editor.degrees(v) : ""
    }

    ColumnLayout {
        id: column
        width: root.width
        spacing: Units.dp(8)

        TimeRangeBar {
            objectName: "dayBar"
            Layout.fillWidth: true
            Layout.bottomMargin: Units.dp(4)
            fontFamily: root.fontFamily
            kind: root.editor.kind
            levels: root.editor.levels
            day: root.editor.day
            defaultSetpoint: root.editor.defaultSetpoint
            defaultOff: root.editor.coolOff
            showNow: root.editor.day === TR.isoDay(new Date())
            nowMinutes: root.editor.nowMinutes
            opacity: root.editor.paused ? 0.5 : 1
        }

        Repeater {
            model: root.segments

            delegate: Rectangle {
                id: periodRow
                required property var modelData
                readonly property string range: TR.hhmm(modelData.start) + " – " + TR.hhmm(modelData.end)

                objectName: "period_" + modelData.start
                Layout.fillWidth: true
                implicitHeight: root.editor.rowHeight
                radius: Units.dp(6)
                color: rowArea.pressed ? Theme.colorAlpha(Theme.whiteColor, 0.1) : Theme.colorAlpha(Theme.whiteColor, 0.04)
                border.color: activeFocus ? Theme.whiteColor : Theme.colorAlpha(Theme.whiteColor, 0.06)
                activeFocusOnTab: true

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("%1, %2, edit").arg(range).arg(root.deg(modelData.setpoint))
                Accessible.onPressAction: root.editor.openSegment(modelData)
                Keys.onReturnPressed: root.editor.openSegment(modelData)
                Keys.onEnterPressed: root.editor.openSegment(modelData)
                Keys.onSpacePressed: root.editor.openSegment(modelData)

                RowLayout {
                    anchors { fill: parent; leftMargin: Units.dp(12); rightMargin: Units.dp(8) }
                    spacing: Units.dp(12)

                    Rectangle {
                        width: Units.dp(14)
                        height: Units.dp(26)
                        radius: Units.dp(3)
                        color: TR.levelColor(root.editor.kind === "cool" ? Theme.blueColor : Theme.yellowColor, periodRow.modelData.level, Theme.mutedColor)
                    }
                    Text {
                        objectName: "periodRange"
                        text: periodRow.range
                        color: Theme.whiteColor
                        font { family: root.fontFamily; pixelSize: Units.dp(17); weight: Font.DemiBold }
                    }
                    Text {
                        objectName: "periodSetpoint"
                        text: root.deg(periodRow.modelData.setpoint)
                        color: Theme.whiteColor
                        font { family: root.fontFamily; pixelSize: Units.dp(18) }
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: qsTr("Edit")
                        color: Theme.blueColor
                        font { family: root.fontFamily; pixelSize: Units.dp(15) }
                    }
                    ClimateIcon {
                        name: "chevronR"
                        size: Units.dp(20)
                        color: Theme.mutedColor
                    }
                }

                MouseArea {
                    id: rowArea
                    anchors.fill: parent
                    onClicked: root.editor.openSegment(periodRow.modelData)
                }
            }
        }

        TimeRangeDashed {
            objectName: "emptyDay"
            visible: root.empty && root.editor.loaded
            Layout.fillWidth: true
            implicitHeight: emptyColumn.implicitHeight + Units.dp(32)
            color: Theme.borderColor

            ColumnLayout {
                id: emptyColumn
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: Units.dp(16) }
                spacing: Units.dp(8)
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No period that day")
                    color: Theme.whiteColor
                    font { family: root.fontFamily; pixelSize: Units.dp(18); weight: Font.DemiBold }
                }
                Text {
                    objectName: "emptyText"
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: !root.cool ? qsTr("Heating stays at %1 all day.").arg(root.deg(root.editor.defaultSetpoint))
                        : root.editor.coolOff ? qsTr("Cooling stays off all day.")
                        : qsTr("Cooling stays at %1 all day.").arg(root.deg(root.editor.defaultSetpoint))
                    color: Theme.mutedColor
                    font { family: root.fontFamily; pixelSize: Units.dp(15) }
                }
                TimeRangeButton {
                    objectName: "addFirstButton"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Units.dp(4)
                    fontFamily: root.fontFamily
                    implicitHeight: root.editor.rowHeight
                    leading: "plus"
                    accent: true
                    text: qsTr("Add a period")
                    enabled: !root.editor.locked
                    onClicked: root.editor.openNew(18 * 60, 22 * 60)
                }
            }
        }

        TimeRangeDashed {
            id: addRow
            objectName: "addButton"
            visible: !root.empty
            Layout.fillWidth: true
            implicitHeight: root.editor.rowHeight
            opacity: enabled ? 1 : 0.4
            enabled: !root.editor.locked
            color: Theme.colorAlpha(Theme.blueColor, 0.7)
            fillColor: addArea.pressed ? Theme.colorAlpha(Theme.blueColor, 0.12) : "transparent"
            activeFocusOnTab: enabled

            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Add a period")
            Accessible.onPressAction: root.editor.openNew(18 * 60, 22 * 60)
            Keys.onReturnPressed: root.editor.openNew(18 * 60, 22 * 60)
            Keys.onEnterPressed: root.editor.openNew(18 * 60, 22 * 60)
            Keys.onSpacePressed: root.editor.openNew(18 * 60, 22 * 60)

            Row {
                anchors.centerIn: parent
                spacing: Units.dp(8)
                ClimateIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "plus"
                    size: Units.dp(20)
                    color: Theme.mutedColor
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Add a period")
                    color: Theme.blueColor
                    font { family: root.fontFamily; pixelSize: Units.dp(17); weight: Font.DemiBold }
                }
            }

            MouseArea {
                id: addArea
                anchors.fill: parent
                onClicked: root.editor.openNew(18 * 60, 22 * 60)
            }
        }

        Rectangle {
            objectName: "restOfTime"
            Layout.fillWidth: true
            Layout.topMargin: Units.dp(4)
            implicitHeight: restRow.implicitHeight + Units.dp(16)
            radius: Units.dp(6)
            color: Theme.colorAlpha(Theme.whiteColor, 0.04)
            border.color: Theme.colorAlpha(Theme.whiteColor, 0.06)

            RowLayout {
                id: restRow
                anchors { fill: parent; margins: Units.dp(8); leftMargin: Units.dp(12) }
                spacing: Units.dp(8)

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Units.dp(2)
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The rest of the time")
                        color: Theme.whiteColor
                        elide: Text.ElideRight
                        font { family: root.fontFamily; pixelSize: Units.dp(16); weight: Font.DemiBold }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("outside periods")
                        color: Theme.mutedColor
                        elide: Text.ElideRight
                        font { family: root.fontFamily; pixelSize: Units.dp(13) }
                    }
                }

                TimeRangeButton {
                    objectName: "coolOffButton"
                    visible: root.cool && root.editor.ios["cool_default_off"] !== undefined
                    fontFamily: root.fontFamily
                    implicitHeight: root.editor.targetSize
                    sidePadding: Units.dp(20)
                    leading: root.editor.coolOff ? "check" : ""
                    text: qsTr("Off")
                    checkable: true
                    checked: root.editor.coolOff
                    enabled: !root.editor.locked
                    Accessible.name: qsTr("Cooling off outside periods")
                    onClicked: root.editor.setCoolOff(!root.editor.coolOff)
                }

                Rectangle {
                    objectName: "defaultStepper"
                    visible: !root.editor.coolOff
                    implicitWidth: stepRow.implicitWidth + Units.dp(4)
                    implicitHeight: root.editor.targetSize
                    radius: Units.dp(6)
                    color: "transparent"
                    border.color: Theme.borderColor

                    Row {
                        id: stepRow
                        anchors.centerIn: parent
                        TimeRangeButton {
                            objectName: "defaultDown"
                            fontFamily: root.fontFamily
                            implicitHeight: root.editor.targetSize
                            color: "transparent"
                            border.width: activeFocus ? 1 : 0
                            text: "−"
                            textSize: Units.dp(28)
                            textColor: Theme.blueColor
                            enabled: !root.editor.locked && root.editor.defaultSetpoint !== null
                            Accessible.name: qsTr("Lower the default setpoint")
                            onClicked: root.editor.stepDefault(-TR.SetpointStep)
                        }
                        Text {
                            objectName: "defaultText"
                            width: Units.dp(64)
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: root.deg(root.editor.defaultSetpoint)
                            color: Theme.whiteColor
                            font { family: root.fontFamily; pixelSize: Units.dp(20) }
                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("Outside periods: %1").arg(text)
                        }
                        TimeRangeButton {
                            objectName: "defaultUp"
                            fontFamily: root.fontFamily
                            implicitHeight: root.editor.targetSize
                            color: "transparent"
                            border.width: activeFocus ? 1 : 0
                            text: "+"
                            textSize: Units.dp(28)
                            textColor: Theme.blueColor
                            enabled: !root.editor.locked && root.editor.defaultSetpoint !== null
                            Accessible.name: qsTr("Raise the default setpoint")
                            onClicked: root.editor.stepDefault(TR.SetpointStep)
                        }
                    }
                }
            }
        }
    }
}
