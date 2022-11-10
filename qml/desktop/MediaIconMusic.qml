import QtQuick
import SharedComponents

Image {
    width: Units.dp(118)
    height: Units.dp(108)

    source: calaosApp.getPictureSized("cd_back")

    Image {

        x: Units.dp(18)
        y: Units.dp(9)

        width: Units.dp(91)
        height: Units.dp(91)

        source: calaosApp.getPictureSized("cd_01")

        NumberAnimation on rotation {
            running: true; from: 0; to: 360;
            loops: Animation.Infinite;
            duration: 1200
        }
    }
}
