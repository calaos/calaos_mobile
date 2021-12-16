import QtQuick
import "Utils.js" as Utils
import SharedComponents

PopupBase {
    id: overlay

    overlayLayer: "myOverlayLayer"
    overlayColor: Qt.rgba(0, 0, 0, 0.4)

    visible: transitionOpacity > 0
    state: showing ? "visible" : "hidden"

    x: (parent.width - width)/2
    y: (parent.height - height)/2

    property alias transitionOpacity: shadow.opacity

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: overlay
                x: sourceView ? sourceView.mapToItem(overlay.parent, 0, 0).x : 0
                y: sourceView ? sourceView.mapToItem(overlay.parent, 0, 0).y : 0
                width: sourceView ? sourceView.width : 0
                height: sourceView ? sourceView.height : 0
            }
        }
    ]

    transitions: Transition {
        from: "*"; to: "*"

        NumberAnimation {
            target: overlay
            properties: "x,y,width,height"
            duration: 300; easing.type: Easing.InOutQuad
        }
    }

    property Item sourceView

    function open(sourceView) {
        overlay.sourceView = sourceView;

        parent = Utils.findRootChild(overlay, overlayLayer)
        showing = true
        forceActiveFocus()
        parent.currentOverlay = overlay

        opened()
    }

    function close() {
        showing = false
        parent.currentOverlay = null
        sourceView = null
    }

    BaseView {
        id: shadow

        anchors.fill: parent
        opacity: showing ? 1 : 0
        elevation: 5

        Behavior on opacity {
            NumberAnimation {
                duration: 300; easing.type: Easing.InOutQuad
            }
        }
    }
}

