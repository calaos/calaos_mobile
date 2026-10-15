import QtQuick
import SharedComponents

Item {

    property alias coverSource: cover.source
    property bool playing: false

    width: Units.dp(118)
    height: Units.dp(63)

    Image {
        width: Units.dp(118)
        height: Units.dp(108)

        source: calaosApp.getPictureSized("cd_back")

        Image {

            x: playing?Units.dp(42):Units.dp(18)
            y: Units.dp(9)

            Behavior on x {
                SequentialAnimation {
                    ScriptAction { script: if (playing) anim.running = playing}
                    NumberAnimation { duration: 500; easing.type: Easing.OutSine}
                    ScriptAction { script: if (!playing) anim.running = playing}
                }
            }

            width: Units.dp(91)
            height: Units.dp(91)

            source: calaosApp.getPictureSized("cd_01")

            NumberAnimation on rotation { id: anim; running: false; from: 0; to: 360; loops: Animation.Infinite; duration: 1200 }
        }

        Image {
            id: cover

            x: Units.dp(18)
            y: Units.dp(9)

            width: Units.dp(91)
            height: Units.dp(91)
        }
    }

}
