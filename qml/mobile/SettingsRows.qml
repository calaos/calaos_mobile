import QtQuick
import SharedComponents

//A framed block of SettingsRow or SettingsRadio
Rectangle {
    id: rows

    default property alias content: rowsColumn.data
    property bool danger: false

    x: Units.dp(16)
    width: (parent ? parent.width : 0) - Units.dp(32)
    height: rowsColumn.implicitHeight + 2 * border.width
    radius: Units.dp(8)
    color: danger ? "transparent" : Theme.colorAlpha(Theme.whiteColor, 0.03)
    border {
        width: Units.dp(2)
        color: danger ? Theme.colorAlpha(Theme.redColor, 0.45) : Theme.borderColor
    }

    Column {
        id: rowsColumn
        x: rows.border.width
        y: rows.border.width
        width: parent.width - 2 * rows.border.width
    }
}
