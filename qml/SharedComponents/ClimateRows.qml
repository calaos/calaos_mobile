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

    Repeater {
        model: root.choices

        delegate: MouseArea {
            id: row

            required property var modelData
            readonly property bool selected: row.modelData.value === root.current

            objectName: "row_" + row.modelData.value
            width: root.width
            height: Units.dp(50)
            enabled: root.active
            onClicked: root.chosen(row.modelData.value)

            Row {
                anchors { left: parent.left; leftMargin: Units.dp(8); verticalCenter: parent.verticalCenter }
                spacing: Units.dp(12)

                ClimateIcon {
                    visible: row.modelData.icon !== undefined
                    name: row.modelData.icon !== undefined ? row.modelData.icon : "help"
                    size: Units.dp(20)
                    color: row.selected ? Theme.blueColor : Theme.mutedColor
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    objectName: "rowLabel"
                    color: row.selected ? Theme.blueColor : Theme.whiteColor
                    font { family: root.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
                    text: row.modelData.label
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
