import QtQuick 2.0
import SharedComponents 1.0

Item {

    property alias coverSource: cover.source
    property bool playing: false

    width: 118 * calaosApp.density
    height: 63 * calaosApp.density

    Image {
        width: 118 * calaosApp.density
        height: 108 * calaosApp.density

        source: calaosApp.getPictureSized("cd_back")

        Image {

            x: playing?42 * calaosApp.density:18 * calaosApp.density
            y: 9 * calaosApp.density

            Behavior on x {
                SequentialAnimation {
                    ScriptAction { script: if (playing) anim.running = playing}
                    NumberAnimation { duration: 500; easing.type: Easing.OutSine}
                    ScriptAction { script: if (!playing) anim.running = playing}
                }
            }

            width: 91 * calaosApp.density
            height: 91 * calaosApp.density

            source: calaosApp.getPictureSized("cd_01")

            NumberAnimation on rotation { id: anim; running: false; from: 0; to: 360; loops: Animation.Infinite; duration: 1200 }
        }

        Image {
            id: cover

            x: 18 * calaosApp.density
            y: 9 * calaosApp.density

            width: 91 * calaosApp.density
            height: 91 * calaosApp.density
        }
    }

}
