import QtQuick
import "."
import "ClimateLogic.js" as Logic

//The body of the settings (UI_CLIMATE.md): the 300-degree dial with the room
//and the status inside, and the setpoint line under it. The bottom sheet and
//the dialog put their own title, mode pill and "more" link around it.
Item {
    id: root

    property var climate
    property string fontFamily
    property real dialSize: Units.dp(280)
    property real numSize: Units.dp(60)
    property real rangeNumSize: Units.dp(34)

    implicitWidth: Math.max(dial.width, reading.implicitWidth)
    implicitHeight: dial.height + Units.dp(4) + reading.implicitHeight

    ClimateDial {
        id: dial
        objectName: "bigDial"
        anchors.horizontalCenter: parent.horizontalCenter
        size: root.dialSize
        stroke: Units.dp(8)
        knobRadius: root.dialSize > Units.dp(260) ? Units.dp(10) : Units.dp(9)
        tickWidth: Units.dp(2)
        ringWidth: Units.dp(2)
        tickReach: 1.3
        ringColor: Theme.backgroundColor

        model: root.climate.dialModel
        fillColor: root.climate.dialFill
        rangeColor: root.climate.rangeColor
        tickColor: root.climate.status.kind === "offline" ? Theme.mutedColor : "#ffffff"
        knobColors: root.climate.knobColors

        Column {
            anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter }
            spacing: Units.dp(4)

            Text {
                objectName: "roomText"
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !isNaN(root.climate.currentTemp)
                color: Theme.whiteColor
                font { family: root.fontFamily; weight: Font.Light; pixelSize: root.dialSize > Units.dp(260) ? Units.dp(22) : Units.dp(20) }
                text: qsTr("Room %1").arg(Logic.formatDegrees(root.climate.currentTemp, root.climate.point, 1))
            }

            Text {
                objectName: "statusWord"
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.climate.toneColor(root.climate.status.tone)
                font { family: root.fontFamily; pixelSize: Units.dp(17) }
                text: root.climate.statusLabel(root.climate.status, true)
            }
        }
    }

    ClimateReading {
        id: reading
        objectName: "reading"
        anchors { top: dial.bottom; topMargin: Units.dp(4); horizontalCenter: parent.horizontalCenter }
        climate: root.climate
        fontFamily: root.fontFamily
        numSize: root.numSize
        rangeNumSize: root.rangeNumSize
    }
}
