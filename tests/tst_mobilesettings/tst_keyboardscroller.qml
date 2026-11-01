import QtQuick
import QtTest
import SharedComponents
import "../../qml/mobile"

//The keyboard is forced through keyboardRect: offscreen has none. It takes the
//bottom of the window, as on iOS where it covers the page.
TestCase {
    id: tc
    name: "KeyboardScroller"
    when: windowShown
    visible: true
    width: 360
    height: 640

    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height

        Item {
            id: content
            width: parent.width
            height: 900

            TextInput {
                id: topField
                y: 16
                width: 200
                height: 48
            }
            TextInput {
                id: field
                y: 500
                width: 200
                height: 48
            }
            Item {
                id: button
                y: 560
                width: 200
                height: 52
            }
            TextInput {
                id: lastField
                y: 840
                width: 200
                height: 48
            }
        }
    }

    TextInput {
        id: outside
        visible: false
    }

    KeyboardScroller {
        id: scroller
        flickable: page
    }

    function keyboard(height) {
        scroller.keyboardRect = height > 0 ? Qt.rect(0, tc.mapToItem(null, 0, tc.height).y - height,
                                                     tc.width, height)
                                           : Qt.rect(0, 0, 0, 0)
    }

    function focusOn(item) {
        item.forceActiveFocus()
        tryVerify(function() { return item.activeFocus })
    }

    function bottomInView(item) {
        return item.mapToItem(page, 0, item.height).y
    }

    function init() {
        scroller.revealTo = null
        keyboard(0)
        page.contentY = 0
        focusOn(topField)
    }

    function test_noKeyboardLeavesThePageAlone() {
        focusOn(field)
        compare(page.contentY, 0)
        compare(page.bottomMargin, 0)
    }

    function test_fieldUnderTheKeyboardComesAboveIt() {
        focusOn(field)
        keyboard(300)
        verify(bottomInView(field) <= page.height - 300)
        compare(page.bottomMargin, 300)
    }

    function test_fieldReachedOnceTheKeyboardIsOpenComesAboveIt() {
        keyboard(300)
        compare(page.contentY, 0)
        focusOn(field)
        verify(bottomInView(field) <= page.height - 300)
    }

    function test_whatFollowsTheFieldIsReachable() {
        scroller.revealTo = button
        focusOn(field)
        keyboard(300)
        verify(bottomInView(button) <= page.height - 300)
    }

    function test_hiddenButtonIsNotAReasonToScroll() {
        button.visible = false
        scroller.revealTo = button
        focusOn(field)
        keyboard(300)
        verify(bottomInView(button) > page.height - 300)
        button.visible = true
    }

    function test_smallAreaKeepsTheFieldFirst() {
        scroller.revealTo = button
        focusOn(field)
        keyboard(560)
        var top = field.mapToItem(page, 0, 0).y
        verify(top >= 0)
        verify(top < page.height - 560)
    }

    function test_fieldAlreadyVisibleDoesNotMove() {
        focusOn(topField)
        keyboard(300)
        compare(page.contentY, 0)
    }

    function test_neverScrollsBackDown() {
        focusOn(field)
        keyboard(300)
        var scrolled = page.contentY
        verify(scrolled > 0)
        focusOn(topField)
        compare(page.contentY, scrolled)
    }

    function test_keyboardNotOverThePageChangesNothing() {
        focusOn(field)
        page.anchors.bottomMargin = 300
        wait(0)
        keyboard(200)
        compare(scroller.covered, 0)
        compare(page.contentY, 0)
        page.anchors.bottomMargin = 0
    }

    function test_focusOutsideThePageDoesNotScroll() {
        outside.visible = true
        outside.forceActiveFocus()
        tryVerify(function() { return outside.activeFocus })
        keyboard(300)
        compare(page.contentY, 0)
        outside.visible = false
    }

    //Past the end of the content: only the margin of the keyboard allows it
    function test_closingTheKeyboardBringsThePageBack() {
        focusOn(lastField)
        keyboard(300)
        verify(page.contentY > page.contentHeight - page.height)
        keyboard(0)
        compare(page.bottomMargin, 0)
        tryVerify(function() { return page.contentY <= page.contentHeight - page.height })
    }
}
