import QtQuick

Item {
    id: scrollableText

    property alias text: textItem.text
    property alias font: textItem.font
    property alias color: textItem.color
    property alias horizontalAlignment: textItem.horizontalAlignment

    // Pixels per second to scroll
    property real pixelsPerSecond: 50

    width: 200
    height: textItem.height
    clip: true

    Text {
        id: textItem
        wrapMode: Text.NoWrap
        anchors.top: parent.top
        width: contentWidth
        x: scrolling ? - scroll_position : 0
    }

    property real scroll_position: 0
    property bool needsScrolling: textItem.contentWidth > scrollableText.width
    property bool scrolling: false

    Timer {
        id: initialDelay
        interval: 2000 //2s delay before scrolling
        running: needsScrolling
        repeat: false
        onTriggered: {
            scrolling = true
            scrollTimer.start()
        }
    }

    Timer {
        id: scrollTimer
        interval: 16
        running: false
        repeat: true
        onTriggered: {
            scroll_position += pixelsPerSecond * (interval / 1000)

            if (scroll_position >= textItem.contentWidth + 20) {
                scrollTimer.stop()
                scrolling = false
                scroll_position = 0

                initialDelay.restart()
            }
        }
    }

    // Reset animation when text changes
    onTextChanged: {
        scroll_position = 0
        scrolling = false
        if (initialDelay.running) initialDelay.restart()
        if (scrollTimer.running) scrollTimer.stop()
        if (needsScrolling) initialDelay.start()
    }
}
