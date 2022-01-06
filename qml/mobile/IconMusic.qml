import QtQuick
import SharedComponents

Item {

    width: 118 * calaosApp.density
    height: 63 * calaosApp.density

    Image {
        width: 118 * calaosApp.density
        height: 108 * calaosApp.density

        source: calaosApp.getPictureSized("cd_back")

        Image {

            x: 18 * calaosApp.density
            y: 9 * calaosApp.density

            width: 91 * calaosApp.density
            height: 91 * calaosApp.density

            source: calaosApp.getPictureSized("cd_01")

            NumberAnimation on rotation { running: true; from: 0; to: 360; loops: Animation.Infinite; duration: 1200 }
        }
    }

}
