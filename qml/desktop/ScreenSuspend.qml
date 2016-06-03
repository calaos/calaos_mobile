import QtQuick 2.5
import SharedComponents 1.0

Item {
    id: suspendLayer
    anchors.fill: parent
    z: 5000

    function startWakeup() {
        screenManager.wakeUpNowWhenScreenOn();
        animSuspend.stop()
        animWakeup.start()
    }

    function startSuspend() {
        animWakeup.stop()
        animSuspend.start()
    }

    Rectangle {
        id: blackScreen
        anchors.fill: parent
        color: "black"
        opacity: 0
        visible: opacity > 0

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: true
            onClicked: startWakeup()
        }
    }

    property bool isDuringWakeup: false
    property bool isDuringSuspend: false

    Connections {
        target: screenManager
        onWakeUpScreen: {
            isDuringWakeup = false;
        }
    }

    Connections {
        target: screenManager
        onWakeUpScreenStart: {
            startWakeup();

            //TODO: Handle dpms_block option here
            //Send action to start keyboard/keypad (for PIN code)
            //to unlock the screen
        }
    }

    Connections {
        target: screenManager
        onSuspendScreen: {
            //TODO: Handle dpms_block option here
            //Close keyboard/keypad
        }
    }

    Connections {
        target: screenManager
        onSuspendScreenStart: {
            startSuspend();
        }
    }

    SequentialAnimation {
        id: animSuspend
        PropertyAnimation {
            duration: 3000
            target: blackScreen
            property: "opacity"
            to: 1
        }
        ScriptAction {
            script: screenManager.suspendNow();
        }
    }

    SequentialAnimation {
        id: animWakeup
        PropertyAnimation {
            duration: 1500
            target: blackScreen
            property: "opacity"
            to: 0
        }
//        ScriptAction {
//            script: {
//                if (isDuringWakeup) {
//                    screenManager.wakeUpNowWhenScreenOn();
//                }
//            }
//        }
    }

    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: true
        hoverEnabled: true
        //onPositionChanged: screenManager.updateTimer()
    }
}
