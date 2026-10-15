import QtQuick
import QtQuick.Window
import "Utils.js" as Utils
import Calaos
import SharedComponents

FocusScope {
    id: popup

    property color overlayColor: "transparent"
    property string overlayLayer: "overlayLayer"
    property bool globalMouseAreaEnabled: true
    property bool dismissOnTap: true
    property bool showing: false
    property Item __lastFocusedItem

    //Dialogs sit on the window overlay, outside the pages, and are not taken
    //away by a lost connection. Only those about the machine (reboot, update,
    //network) must outlive it: the server going away is their normal course.
    property bool closeOnDisconnect: true

    Connections {
        target: calaosApp
        function onApplicationStatusChanged(status) {
            if (popup.closeOnDisconnect && popup.showing && status !== Common.LoggedIn)
                popup.close()
        }
    }

    signal opened
    signal closed

    function toggle(widget) {
        if (showing) {
            close()
        } else {
            open(widget)
        }
    }

    function open() {
        __lastFocusedItem = Window.activeFocusItem
        parent = Utils.findRootChild(popup, overlayLayer)

        if (!parent.enabled)
            return

        showing = true
        forceActiveFocus()
        parent.currentOverlay = popup

        opened()
    }

    function close() {
        showing = false

        if (parent.hasOwnProperty("currentOverlay")) {
            parent.currentOverlay = null
        }

        if (__lastFocusedItem !== null) {
            __lastFocusedItem.forceActiveFocus()
        }

        closed()
    }
}

