import QtQuick
import SharedComponents

MouseArea {
    id: buttonRound
    property bool disabled: false
    property bool next: true

    signal buttonClicked()

    width: btNormal.width
    height: btNormal.height

    state: "released"

    onClicked: if (!disabled) buttonClicked()
    onPressed: buttonRound.state = "pressed"
    onReleased: buttonRound.state = "released"

    Image {
        id: btNormal
        source: next?"qrc:/img/button_round_next.png":
                      "qrc:/img/button_round_previous.png"

        opacity: buttonRound.disabled?0:1.0
        Behavior on opacity { PropertyAnimation { } }
        visible: opacity > 0
    }

    Image {
        id: btNormalGlow
        source: next?"qrc:/img/button_round_next_glow.png":
                      "qrc:/img/button_round_previous_glow.png"

        state: buttonRound.disabled? "released": buttonRound.state

        states: [
            State { name: "released"; PropertyChanges { target: btNormalGlow; opacity: 0.0 } },
            State { name: "pressed"; PropertyChanges { target: btNormalGlow; opacity: 1.0 } }
        ]
        transitions: [
            Transition {
                from: "released"
                to: "pressed"
                PropertyAnimation { duration: 50; properties: "opacity"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "pressed"
                to: "released"
                PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.InCubic }
            }
        ]

        opacity: buttonRound.disabled?0:1.0
        Behavior on opacity { PropertyAnimation { } }
        visible: opacity > 0
    }

    Image {
        id: btOff
        source: next?"qrc:/img/button_round_next_off.png":
                      "qrc:/img/button_round_previous_off.png"
        opacity: buttonRound.disabled?1.0:0
        Behavior on opacity { PropertyAnimation { } }
        visible: opacity > 0
    }

    Image {
        id: btOffGlow
        source: "qrc:/img/button_round_stop.png"

        state: buttonRound.disabled? buttonRound.state: "released"

        states: [
            State { name: "released"; PropertyChanges { target: btOffGlow; opacity: 0.0 } },
            State { name: "pressed"; PropertyChanges { target: btOffGlow; opacity: 1.0 } }
        ]
        transitions: [
            Transition {
                from: "released"
                to: "pressed"
                PropertyAnimation { duration: 50; properties: "opacity"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "pressed"
                to: "released"
                PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.InCubic }
            }
        ]

        opacity: buttonRound.disabled?1.0:0
        Behavior on opacity { PropertyAnimation { } }
        visible: opacity > 0
    }
}
