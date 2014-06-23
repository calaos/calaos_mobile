import QtQuick 2.0

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

    Image {
        id: picon
        source: imageFilenameOn.arg(currentImage)
        opacity: iconState?1:0
    }

    Image {
        id: picoff
        source: imageFilenameOff
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
