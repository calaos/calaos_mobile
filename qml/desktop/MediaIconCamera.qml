import QtQuick
import SharedComponents

Image {
    width: Units.dp(108)
    height: Units.dp(108)

    source: "qrc:/img/menu_media_icon_camera_back.png"

    Image {
        width: Units.dp(108)
        height: Units.dp(108)

        source: "qrc:/img/menu_media_icon_camera_red.png"

        NumberAnimation on opacity {
            easing.amplitude: 2; easing.type: Easing.OutBounce
            running: true; from: 0; to: 1;
            loops: Animation.Infinite; duration: 900
        }
    }
}
