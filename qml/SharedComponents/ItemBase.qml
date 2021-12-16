import QtQuick
import "."

CalaosItemBase {
    id: baseItem

    width: parent.width

    //default 1 row height
    height: 40 * calaosApp.density
    enabled: false

    function feedbackAnim() {
        __inBorder.state = "pressed"
        animTimer.restart()
    }

    Timer {
        id: animTimer
        interval: 300
        repeat: false
        running: false
        triggeredOnStart: false
        onTriggered: {
            __inBorder.state = "released"
        }
    }
}

