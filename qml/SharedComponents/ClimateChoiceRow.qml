pragma ComponentBehavior: Bound

import QtQuick
import "."

//Row of exclusive choices (modes, fan speeds, swing positions). Same border and
//colors as CalaosButton, plus a filled state for the current choice.
Flow {
    id: root

    //[{ value: "heat", label: "Heat" }, ...]
    property var choices: []
    property string current: ""
    property bool active: true

    signal chosen(string value)

    spacing: Units.dp(5)

    Repeater {
        model: root.choices

        delegate: MouseArea {
            id: chip

            required property var modelData
            readonly property bool selected: chip.modelData.value === root.current

            width: Math.max(Units.dp(48), label.implicitWidth + Units.dp(18))
            height: Units.dp(30)
            enabled: root.active
            opacity: root.active ? 1.0 : 0.4

            onClicked: root.chosen(chip.modelData.value)

            Rectangle {
                anchors.fill: parent
                radius: Units.dp(6)
                color: chip.selected ? Theme.colorAlpha(Theme.blueColor, 0.35)
                                     : (chip.pressed ? Theme.colorAlpha(Theme.blueColor, 0.12)
                                                     : "transparent")
                border.width: Units.dp(2)
                border.color: chip.selected ? Theme.blueColor
                                            : Theme.colorAlpha(Theme.blueColor, 0.45)
            }

            Text {
                id: label
                anchors.centerIn: parent
                text: chip.modelData.label
                color: chip.selected ? Theme.whiteColor : Theme.blueColor
                font { family: calaosFont.fontFamilyThin; pointSize: 11 }
            }
        }
    }
}
