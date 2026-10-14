pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import "."
import "ClimateLogic.js" as Logic

//Ventilation, swing and humidity, only what the capabilities announce. A choice
//is sent at once and the page stays open. `columns` is 2 on the desktop, where
//five fan speeds and four swing positions would not fit one under the other.
GridLayout {
    id: root

    property var climate
    property string fontFamily
    property int columnCount: 1

    columns: columnCount
    uniformCellWidths: true
    columnSpacing: Units.dp(24)
    rowSpacing: Units.dp(8)

    component Section: Text {
        Layout.fillWidth: true
        color: Theme.blueColor
        font { family: root.fontFamily; bold: true; pixelSize: Units.dp(16) }
        leftPadding: Units.dp(8)
        bottomPadding: Units.dp(6)

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: Units.dp(2)
            color: "#424242"
        }
    }

    ColumnLayout {
        objectName: "fanSection"
        visible: root.climate.caps.fanModes.length > 0
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        spacing: 0

        Section { text: qsTr("Ventilation") }
        ClimateRows {
            objectName: "fanRows"
            Layout.fillWidth: true
            fontFamily: root.fontFamily
            active: root.climate.controlsActive
            choices: root.climate.toChoices(root.climate.caps.fanModes, root.climate.optionLabel)
            current: String(root.climate.shown("fan_mode"))
            onChosen: function(v) { root.climate.commit({ fan_mode: v }) }
        }
    }

    ColumnLayout {
        objectName: "swingSection"
        visible: root.climate.caps.swingModes.length > 0
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        spacing: 0

        Section { text: qsTr("Swing") }
        ClimateRows {
            objectName: "swingRows"
            Layout.fillWidth: true
            fontFamily: root.fontFamily
            active: root.climate.controlsActive
            choices: root.climate.toChoices(root.climate.caps.swingModes, root.climate.optionLabel)
            current: String(root.climate.shown("swing_mode"))
            onChosen: function(v) { root.climate.commit({ swing_mode: v }) }
        }
    }

    Text {
        objectName: "humidityText"
        visible: Logic.humidityShown(root.climate.caps, root.climate.devState)
        Layout.columnSpan: root.columnCount
        Layout.fillWidth: true
        leftPadding: Units.dp(8)
        color: Theme.mutedColor
        font { family: root.fontFamily; weight: Font.Light; pixelSize: Units.dp(16) }
        text: visible ? qsTr("Humidity %1 %").arg(Logic.plainNumber(root.climate.devState.humidity, root.climate.point, 0)) : ""
    }
}
