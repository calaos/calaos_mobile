import QtQuick
import "."
import "../quickflux"
import "ClimateLogic.js" as Logic

//Climate device in a list (UI_CLIMATE.md): the two-row item. A tap asks the
//variant's main view to open the settings, a bottom sheet on the phone and a
//dialog on the desktop.
ItemBase {
    id: root

    property variant modelData

    property alias clock: climate.clock
    readonly property alias climate: climate
    readonly property alias lockoutTicking: climate.lockoutTicking

    height: head.implicitHeight

    ClimateModel {
        id: climate
        modelData: root.modelData
    }

    ClimateListItem {
        id: head
        anchors { left: parent.left; right: parent.right; top: parent.top }

        fontFamily: calaosFont.fontFamily
        name: climate.name
        nameActive: climate.status.acting
        roomText: Logic.formatRoom(climate.devState ? climate.devState.current_temp : undefined, climate.point)
        modeVisible: climate.deviceKnown ? true : climate.mode !== ""
        modeText: climate.mode !== "" ? climate.modeLabel(climate.mode) : qsTr("Mode unknown")
        modeIcon: climate.modeIcon(climate.mode)
        modeMuted: climate.status.modeMuted
        statusText: climate.statusLabel(climate.status, false)
        statusColor: climate.toneColor(climate.status.tone)
        setpointText: Logic.formatListSetpoint(climate.setpointKind, climate.values, climate.point,
                                               climate.caps.bounds ? climate.decimals : undefined)
        setpointMuted: Logic.setpointMuted(climate.status, climate.mode)

        onClicked: AppActions.showClimateDetails(root.modelData)
        onPressedChanged: root.__inBorder.state = pressed ? "pressed" : "released"
        onHoveredChanged: root.state = hovered ? "hovered" : "normal"

        dial.model: climate.dialModel
        dial.fillColor: climate.dialFill
        dial.rangeColor: climate.rangeColor
        dial.tickColor: climate.status.kind === "offline" ? Theme.mutedColor : "#ffffff"
        dial.knobColors: climate.knobColors
    }
}
