import QtQuick
import "."

Item {

    property int currentImage: -1
    property int countImage: 9
    property string imageFilenameOn
    property string imageFilenameOff
    property alias sourceWidth: picoff.sourceSize.width
    property alias sourceHeight: picoff.sourceSize.height
    property bool iconState

    signal animationFinished()

    onIconStateChanged: {
        if (iconState)
        {
            currentImage = 1
            tmr.start()
        }
        else {
            tmr.stop()
            animationFinished()
        }
    }

    width: picoff.width
    height: picoff.height

    IconItem {
        id: picon
        // Only build a frame name for a valid index (currentImage is -1 until the animation starts)
        source: currentImage >= 0 ? calaosApp.getPictureSized(imageFilenameOn.arg(currentImage)) : ""
        opacity: iconState?1:0
    }

    IconItem {
        id: picoff
        source: calaosApp.getPictureSized(imageFilenameOff)
        opacity: iconState?0:1
    }

    Timer {
        id: tmr
        interval: 40
        running: false
        repeat: true
        onTriggered: {
            currentImage++
            if (currentImage == countImage) {
                tmr.stop()
                animationFinished()
            }
        }
    }
}
