import QtQuick
import SharedComponents

//Keeps the focused field of a page above the virtual keyboard, and what
//follows it (revealTo) reachable: the keyboard covers the window on iOS.
Item {
    id: scroller

    required property Flickable flickable
    property Item revealTo: null

    // Qt.inputMethod is seen as a plain QObject by the linter
    // qmllint disable missing-property
    property rect keyboardRect: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle
                                                       : Qt.rect(0, 0, 0, 0)
    // qmllint enable missing-property

    readonly property real covered: keyboardRect.height > 0 ?
                                        Math.max(0, flickable.mapToItem(null, 0, flickable.height).y -
                                                    keyboardRect.y) : 0
    readonly property Item focusItem: Window.activeFocusItem

    function isInPage(item) {
        for (let it = item; it; it = it.parent) {
            if (it === flickable.contentItem)
                return true
        }
        return false
    }

    function reveal() {
        if (covered <= 0 || !isInPage(focusItem))
            return

        const margin = Units.dp(8)
        const content = flickable.contentItem
        const last = revealTo && revealTo.visible ? revealTo : focusItem
        const fieldTop = focusItem.mapToItem(content, 0, 0).y
        const bottom = last.mapToItem(content, 0, last.height).y
        const wanted = bottom + margin - (flickable.height - covered)

        //A small visible area may not hold both: the field comes first
        const target = Math.min(wanted, fieldTop - margin)
        if (target > flickable.contentY)
            flickable.contentY = target
    }

    onCoveredChanged: {
        if (covered > 0)
            reveal()
        else
            flickable.returnToBounds()
    }
    onFocusItemChanged: reveal()

    Binding {
        target: scroller.flickable
        property: "bottomMargin"
        value: scroller.covered
    }
}
