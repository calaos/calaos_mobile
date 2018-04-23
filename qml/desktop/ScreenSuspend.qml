import QtQuick 2.5
import SharedComponents 1.0
import QuickFlux 1.0
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
            if (screenManager.dpmsEnabled) {
                blackScreen.state = "asleep"
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        hoverEnabled: true
        onPositionChanged: {
            timerSuspend.restart()
            mouse.accepted = false
        }
        onClicked: mouse.accepted = false
        onDoubleClicked: mouse.accepted = false
        onPressAndHold: mouse.accepted = false
        onPressed: mouse.accepted = false
        onReleased: mouse.accepted = false
        onWheel: wheel.accepted = false
    }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.suspendScreen
            onDispatched: blackScreen.state = "asleep"
        }
        Filter {
            type: ActionTypes.wakeupScreen
            onDispatched: blackScreen.state = "awake"
        }
    }
}
