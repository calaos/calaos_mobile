import QtQuick
import SharedComponents

Item {

    width: 108 * calaosApp.density
    height: 63 * calaosApp.density

    Image {
        width: 108 * calaosApp.density
        height: 108 * calaosApp.density

        source: "qrc:/img/menu_media_icon_camera_back.png"

        Image {
            width: 108 * calaosApp.density
            height: 108 * calaosApp.density

            source: "qrc:/img/menu_media_icon_camera_red.png"

            NumberAnimation
            on opacity { easing.amplitude: 2; easing.type: Easing.OutBounce; running: true; from: 0; to: 1; loops: Animation.Infinite; duration: 900 }

        }
    }

}
