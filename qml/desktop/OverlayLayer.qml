import QtQuick
import Calaos
import SharedComponents

Rectangle {
    id: overlayLayer
    objectName: "overlayLayer"

    anchors.fill: parent

    property Item currentOverlay

    //Open popups, the last one is on top. Closing the top one gives the veil
    //and the clicks back to the one below, which is still on screen.
    property var stack: []

    function raise(popup) {
        stack = stack.filter(function(p) { return p !== popup }).concat([popup])
        currentOverlay = popup
    }

    function release(popup) {
        stack = stack.filter(function(p) { return p !== popup })
        currentOverlay = stack.length > 0 ? stack[stack.length - 1] : null
    }

    //Top first, so that each one hands the focus back to the one under it
    Connections {
        target: calaosApp
        function onApplicationStatusChanged(status) {
            if (status === Common.LoggedIn)
                return
            var open = overlayLayer.stack.slice().reverse()
            for (var i = 0; i < open.length; i++) {
                if (open[i].closeOnDisconnect && open[i].showing)
                    open[i].close()
            }
        }
    }
    color: "transparent"

    onEnabledChanged: {
        if (!enabled && overlayLayer.currentOverlay != null)
            overlayLayer.currentOverlay.close()
    }

    states: State {
        name: "ShowState"
        when: overlayLayer.currentOverlay != null

        PropertyChanges {
            target: overlayLayer
            color: currentOverlay.overlayColor
        }
    }

    transitions: Transition {
        ColorAnimation {
            duration: 300
            easing.type: Easing.InOutQuad
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: overlayLayer.currentOverlay != null &&
                overlayLayer.currentOverlay.globalMouseAreaEnabled
        hoverEnabled: enabled

        onWheel: wheel.accepted = true

        onClicked: {
            if (overlayLayer.currentOverlay.dismissOnTap)
                overlayLayer.currentOverlay.close()
        }
    }
}
