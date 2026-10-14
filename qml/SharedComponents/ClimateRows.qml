pragma ComponentBehavior: Bound

import QtQuick
import "."

//Rows of one choice among several (mode, fan speed, swing position): the
//current one in cyan with a check, a thin rule between rows.
Column {
    id: root

    property string fontFamily
    //[{ value: "heat", label: "Heat", icon: "flame" }, ...]; icon is optional
    property var choices: []
    property string current: ""
    property bool active: true

    signal chosen(string value)

    //The delegates are kept as long as the list of values is: `choices` is
    //recomputed whenever the device publishes, and rows rebuilt under the
    //finger would swallow a tap.
    ListModel { id: rows }

    function sync() {
        var same = rows.count === choices.length
        for (var i = 0; same && i < choices.length; i++)
            same = rows.get(i).value === choices[i].value
        if (!same) {
            rows.clear()
            for (var j = 0; j < choices.length; j++)
                rows.append({ value: choices[j].value, label: choices[j].label,
                              icon: choices[j].icon !== undefined ? choices[j].icon : "" })
        } else {
            for (var k = 0; k < choices.length; k++)
                rows.setProperty(k, "label", choices[k].label)
        }
    }

    onChoicesChanged: sync()
    Component.onCompleted: sync()

    Repeater {
        model: rows

        delegate: MouseArea {
            id: row

            required property string value
            required property string label
            required property string icon
            readonly property bool selected: row.value === root.current

            objectName: "row_" + row.value
            width: root.width
            height: Units.dp(50)
            enabled: root.active
            onClicked: root.chosen(row.value)

            Row {
                anchors { left: parent.left; leftMargin: Units.dp(8); verticalCenter: parent.verticalCenter }
                spacing: Units.dp(12)

                ClimateIcon {
                    visible: row.icon !== ""
                    name: row.icon !== "" ? row.icon : "help"
                    size: Units.dp(20)
                    color: row.selected ? Theme.blueColor : Theme.mutedColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    objectName: "rowLabel"
                    color: row.selected ? Theme.blueColor : Theme.whiteColor
                    font { family: root.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
                    text: row.label
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            ClimateIcon {
                objectName: "rowCheck"
                visible: row.selected
                name: "check"
                size: Units.dp(22)
                color: Theme.blueColor
                anchors { right: parent.right; rightMargin: Units.dp(8); verticalCenter: parent.verticalCenter }
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
            }
        }
    }
}
