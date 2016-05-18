import QtQuick 2.0
import "."

Item {

    property int currentImage: -1
    property int countImage: 9
    property string imageFilenameOn
    property string imageFilenameOff
    property alias sourceWidth: picoff.sourceSize.width
    property alias sourceHeight: picoff.sourceSize.height
    property bool iconState

    onIconStateChanged: {
        if (iconState)
        {
            currentImage = 1
            tmr.start()
        }
        else
            tmr.stop()
    }

    width: picoff.width
    height: picoff.height

    IconItem {
        id: picon
        source: calaosApp.getPictureSized(imageFilenameOn.arg(currentImage))
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
            if (currentImage == countImage)
                tmr.stop()
        }
    }
}
