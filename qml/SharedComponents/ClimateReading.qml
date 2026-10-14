pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."
import "ClimateLogic.js" as Logic

//The line under the dial: "- 22° +", or two groups, yellow for the low end and
//cyan for the high one, in auto. At min_range the two inner buttons are
//refused and the gap is said under the line.
Item {
    id: root

    property var climate
    property string fontFamily
    property real numSize: Units.dp(48)
    property real rangeNumSize: Units.dp(34)

    readonly property bool ranged: climate.setpointKind === "range"
    readonly property bool shown: climate.deviceKnown && climate.setpointKind !== "none"

    visible: shown
    implicitWidth: row.implicitWidth
    implicitHeight: shown ? col.implicitHeight : 0

    component Step: Item {
        id: step
        required property string which
        required property int dir
        property string face: dir < 0 ? "button_min" : "button_plus"

        width: Units.dp(44)
        height: Units.dp(48)
        enabled: root.climate.canStep(which, dir)
        opacity: enabled ? 1 : 0.35

        ItemButtonAction {
            objectName: "face"
            anchors.centerIn: parent
            imageSource: step.face
            onButtonClicked: root.climate.step(step.which, step.dir)
        }
    }

    component Num: Text {
        font { family: root.fontFamily; weight: Font.Light }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Column {
        id: col
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Units.dp(6)

        Row {
            id: row
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: root.ranged ? Units.dp(14) : Units.dp(8)

            Row {
                visible: !root.ranged
                Step { objectName: "btMin"; which: "target"; dir: -1 }
                Num {
                    objectName: "valueText"
                    width: Units.dp(130); height: Units.dp(48)
                    font.pixelSize: root.numSize
                    color: Logic.setpointMuted(root.climate.status, root.climate.mode) ? Theme.mutedColor : Theme.yellowColor
                    text: Logic.formatListSetpoint("single", root.climate.values, root.climate.point, root.climate.decimals)
                }
                Step { objectName: "btPlus"; which: "target"; dir: 1 }
            }

            Row {
                visible: root.ranged
                Step { objectName: "btLowMin"; which: "low"; dir: -1 }
                Num {
                    objectName: "lowText"
                    width: Units.dp(80); height: Units.dp(48)
                    font.pixelSize: root.rangeNumSize
                    color: Theme.yellowColor
                    text: Logic.formatDegrees(root.climate.values.low, root.climate.point, root.climate.decimals)
                }
                Step { objectName: "btLowPlus"; which: "low"; dir: 1 }
            }

            Row {
                visible: root.ranged
                Step { objectName: "btHighMin"; which: "high"; dir: -1 }
                Num {
                    objectName: "highText"
                    width: Units.dp(80); height: Units.dp(48)
                    font.pixelSize: root.rangeNumSize
                    color: Theme.blueColor
                    text: Logic.formatDegrees(root.climate.values.high, root.climate.point, root.climate.decimals)
                }
                Step { objectName: "btHighPlus"; which: "high"; dir: 1 }
            }
        }

        Text {
            objectName: "stopText"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.climate.rangeAtStop
            color: Theme.mutedColor
            font { family: root.fontFamily; pixelSize: Units.dp(14) }
            text: qsTr("Minimum gap %1°").arg(Logic.plainNumber(root.climate.caps.bounds ? root.climate.caps.bounds.minRange : 0,
                                                                root.climate.point, root.climate.decimals))
        }
    }
}
