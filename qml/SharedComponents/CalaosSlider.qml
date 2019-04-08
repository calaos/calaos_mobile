import QtQuick 2.4
import QtQuick.Controls 1.4 as Controls
import "."

Controls.Slider {
    id: slider

    tickmarksEnabled: false

    implicitHeight: orientation === Qt.Vertical? Units.dp(200): Units.dp(40)
    implicitWidth: orientation === Qt.Vertical? Units.dp(40): Units.dp(200)

    property Component customBackground: null

    style: CalaosSliderStyle {
        customBackground: slider.customBackground
    }
}
