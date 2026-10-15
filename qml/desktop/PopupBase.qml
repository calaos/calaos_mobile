import QtQuick
import QtQuick.Window
import "Utils.js" as Utils
import SharedComponents

FocusScope {
    id: popup

    property color overlayColor: "transparent"
    property string overlayLayer: "overlayLayer"
    property bool globalMouseAreaEnabled: true
    property bool dismissOnTap: true
    property bool showing: false
    property Item __lastFocusedItem

    //Set by the dialogs showing data of the session (an io, a device): a lost
    //connection leaves them without anything to show. Pages stay on the desktop
    //and a reconnect is routine, so every other dialog, and the text a user is
    //typing in it, must survive it.
    property bool closeOnDisconnect: false

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
        if (parent.raise)
            parent.raise(popup)
        else
            parent.currentOverlay = popup

        opened()
    }

    function close() {
        showing = false

        if (parent.release)
            parent.release(popup)
        else if (parent.hasOwnProperty("currentOverlay"))
            parent.currentOverlay = null

        if (__lastFocusedItem !== null) {
            __lastFocusedItem.forceActiveFocus()
        }

        closed()
    }
}

