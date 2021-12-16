import QtQuick
import SharedComponents
import QuickFlux
import "../quickflux"

Item {
    id: suspendLayer
    anchors.fill: parent
    z: 5000

    Rectangle {
        id: blackScreen
        anchors.fill: parent
        color: "black"
        opacity: 0
        visible: opacity > 0

        MouseArea {
            anchors.fill: parent
            onClicked: blackScreen.state = "awake"
        }

        state: "awake"

        states: [
            State {
                name: "awake"; PropertyChanges { target: blackScreen; opacity: 0 }
            },
            State {
                name: "asleep"; PropertyChanges { target: blackScreen; opacity: 0.6 }
            }
        ]

        transitions: [
            Transition {
                from: "awake"
                to: "asleep"
                SequentialAnimation {
                    PropertyAnimation { duration: 3000; properties: "opacity"; easing.type: Easing.InQuart }
                    ScriptAction { script: screenManager.suspendScreen(); }
                }
            },
            Transition {
                from: "asleep"
                to: "awake"
                SequentialAnimation {
                    ScriptAction { script: screenManager.wakeupScreen(); }
                    PropertyAnimation { duration: 1500; properties: "opacity"; easing.type: Easing.OutQuad }
                    ScriptAction { script: timerSuspend.restart(); }
                }
            }
        ]
    }

    Timer {
        id: timerSuspend
        interval: screenManager.dpmsTime
        repeat: false
        running: true && screenManager.dpmsEnabled
        triggeredOnStart: false
        onTriggered: {
            console.log("**** timer trigger")
            if (screenManager.dpmsEnabled) {
                console.log("**** sleep screen")
                blackScreen.state = "asleep"
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        hoverEnabled: true
        onPositionChanged: (mouse) => {
            timerSuspend.restart()
            mouse.accepted = false
        }
        onClicked: (mouse) => { mouse.accepted = false }
        onDoubleClicked: (mouse) => { mouse.accepted = false }
        onPressAndHold: (mouse) => { mouse.accepted = false }
        onPressed: (mouse) => { mouse.accepted = false }
        onReleased: (mouse) => { mouse.accepted = false }
        onWheel: (mouse) => { wheel.accepted = false }
    }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.suspendScreen
            onDispatched: (filtertype, message) => {
                console.log("### dispatch: screen sleep")
                blackScreen.state = "asleep"
            }
        }
        Filter {
            type: ActionTypes.wakeupScreen
            onDispatched: (filtertype, message) => {
                              console.log("### dispatch: screen wake")
                blackScreen.state = "awake"
            }
        }
    }
}
