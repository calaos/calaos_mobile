import QtQuick 2.4
import QtQuick.Controls 1.4 as Controls

Controls.Slider {
    id: slider

    tickmarksEnabled: false

    implicitHeight: 40 * calaosApp.density
    implicitWidth: 200 * calaosApp.density

    property Component customBackground: null

    style: CalaosSliderStyle {
        customBackground: slider.customBackground
    }
}
