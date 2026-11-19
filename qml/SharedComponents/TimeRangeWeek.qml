pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import "."
import "TimeRangeLogic.js" as TR

//Week page of the program editor: one row per day, the bar is a preview and
//the whole row opens the day.
Flickable {
    id: root

    readonly property string page: "week"
    property Item editor
    readonly property string fontFamily: editor ? editor.fontFamily : ""
    readonly property bool phone: editor ? editor.phone : false
    readonly property real nameWidth: Units.dp(phone ? 44 : 110)

    contentHeight: column.implicitHeight + Units.dp(8)
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: column
        width: root.width
        spacing: Units.dp(8)

        Rectangle {
            objectName: "programStatus"
            visible: root.editor.hasPause && !root.editor.paused
            Layout.fillWidth: true
            implicitHeight: activeRow.implicitHeight + Units.dp(12)
            radius: Units.dp(6)
            color: Theme.colorAlpha(Theme.whiteColor, 0.04)
            border.color: Theme.colorAlpha(Theme.whiteColor, 0.06)

            RowLayout {
                id: activeRow
                anchors { fill: parent; margins: Units.dp(6); leftMargin: Units.dp(12) }
                spacing: Units.dp(10)
                Rectangle {
                    width: Units.dp(8)
                    height: width
                    radius: width / 2
                    color: Theme.greenColor
                }
                Text {
                    objectName: "activeText"
                    Layout.fillWidth: true
                    text: qsTr("Schedule active")
                    color: Theme.whiteColor
                    elide: Text.ElideRight
                    font { family: root.fontFamily; pixelSize: Units.dp(16) }
                }
                TimeRangeButton {
                    objectName: "pauseButton"
                    fontFamily: root.fontFamily
                    implicitHeight: root.editor.targetSize
                    sidePadding: Units.dp(20)
                    leading: "pause"
                    text: qsTr("Pause")
                    enabled: !root.editor.locked
                    onClicked: root.editor.setPaused(true)
                }
            }
        }

        TimeRangeDashed {
            objectName: "pausedBanner"
            visible: root.editor.paused
            Layout.fillWidth: true
            implicitHeight: pausedRow.implicitHeight + Units.dp(12)
            color: Theme.amberColor
            fillColor: Theme.colorAlpha(Theme.amberColor, 0.08)
            Accessible.role: Accessible.AlertMessage
            Accessible.name: pausedText.text

            RowLayout {
                id: pausedRow
                anchors { fill: parent; margins: Units.dp(6); leftMargin: Units.dp(12) }
                spacing: Units.dp(10)
                ClimateIcon {
                    name: "pause"
                    size: Units.dp(18)
                    color: Theme.amberColor
                }
                Text {
                    id: pausedText
                    objectName: "pausedText"
                    Layout.fillWidth: true
                    text: qsTr("Paused: setpoint stays at %1")
                          .arg(root.editor.degrees(root.editor.holdSetpoint !== undefined && root.editor.holdSetpoint !== null
                                                   ? root.editor.holdSetpoint : root.editor.defaultSetpoint))
                    color: Theme.whiteColor
                    wrapMode: Text.WordWrap
                    font { family: root.fontFamily; pixelSize: Units.dp(16) }
                }
                TimeRangeButton {
                    objectName: "resumeButton"
                    fontFamily: root.fontFamily
                    implicitHeight: root.editor.targetSize
                    sidePadding: Units.dp(20)
                    leading: "play"
                    text: qsTr("Resume")
                    enabled: !root.editor.locked
                    onClicked: root.editor.setPaused(false)
                }
            }
        }

        Text {
            objectName: "loadingText"
            visible: root.editor.loading && !root.editor.loaded
            text: qsTr("Loading the program…")
            color: Theme.mutedColor
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }

        Text {
            objectName: "noProgramText"
            visible: !root.editor.kinds.length
            text: qsTr("This climate has no program.")
            color: Theme.mutedColor
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
        }

        Text {
            objectName: "offlineText"
            visible: root.editor.offline || root.editor.readOnly
            Layout.fillWidth: true
            text: qsTr("Offline: the schedule is read-only")
            color: Theme.amberColor
            wrapMode: Text.WordWrap
            font { family: root.fontFamily; pixelSize: Units.dp(15) }
        }

        Repeater {
            model: root.editor.kinds.length ? 7 : 0

            delegate: Rectangle {
                id: dayRow
                required property int index
                readonly property int day: index + 1
                readonly property int count: root.editor.periodCount(day)

                objectName: "dayRow" + day
                Layout.fillWidth: true
                implicitHeight: root.editor.rowHeight
                radius: Units.dp(6)
                color: rowArea.pressed ? Theme.colorAlpha(Theme.whiteColor, 0.1) : Theme.colorAlpha(Theme.whiteColor, 0.04)
                border.color: activeFocus ? Theme.whiteColor : Theme.colorAlpha(Theme.whiteColor, 0.06)
                activeFocusOnTab: true

                Accessible.role: Accessible.Button
                Accessible.name: root.editor.dayName(day) + ", " + root.editor.countText(count)
                Accessible.onPressAction: root.editor.openDay(day)
                Keys.onReturnPressed: root.editor.openDay(day)
                Keys.onEnterPressed: root.editor.openDay(day)
                Keys.onSpacePressed: root.editor.openDay(day)

                RowLayout {
                    anchors { fill: parent; leftMargin: Units.dp(12); rightMargin: Units.dp(8) }
                    spacing: Units.dp(10)

                    Text {
                        Layout.preferredWidth: root.nameWidth - Units.dp(12)
                        text: root.phone ? root.editor.dayShort(dayRow.day) : root.editor.dayName(dayRow.day)
                        color: Theme.whiteColor
                        elide: Text.ElideRight
                        font { family: root.fontFamily; pixelSize: Units.dp(16); weight: Font.Medium }
                    }

                    TimeRangeBar {
                        objectName: "weekBar" + dayRow.day
                        Layout.fillWidth: true
                        fontFamily: root.fontFamily
                        barHeight: Units.dp(26)
                        showAxis: false
                        opacity: root.editor.paused ? 0.5 : 1
                        kind: root.editor.kind
                        levels: root.editor.levels
                        day: dayRow.day
                        defaultSetpoint: root.editor.defaultSetpoint
                        defaultOff: root.editor.coolOff

                        Text {
                            objectName: "emptyHint"
                            visible: dayRow.count === 0 && root.editor.loaded
                            anchors { left: parent.left; leftMargin: Units.dp(10); verticalCenter: parent.verticalCenter }
                            width: parent.width - Units.dp(20)
                            text: qsTr("No period: tap to add")
                            color: Theme.whiteColor
                            elide: Text.ElideRight
                            font { family: root.fontFamily; pixelSize: Units.dp(13) }
                        }
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
                    onClicked: root.editor.openDay(dayRow.day)
                }
            }
        }

        Item {
            objectName: "weekAxis"
            visible: root.editor.kinds.length > 0
            Layout.fillWidth: true
            implicitHeight: Units.dp(18)

            Item {
                id: axisTrack
                anchors { fill: parent; leftMargin: root.nameWidth + Units.dp(10); rightMargin: Units.dp(8 + 20 + 10) }

                Repeater {
                    model: [0, 6, 12, 18, 24]
                    delegate: Text {
                        required property int modelData
                        x: Math.max(0, Math.min(axisTrack.width - width,
                                                axisTrack.width * modelData / 24 - width / 2))
                        text: modelData + " h"
                        color: Theme.mutedColor
                        font { family: root.fontFamily; pixelSize: Units.dp(12) }
                    }
                }
            }
        }
    }
}
