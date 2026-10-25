import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import "../../qml/mobile"

//The admin code sheet over a stand-in for AdminCodeModel (its logic has its
//own C++ test, tst_admincode): opened by the model only, digits and
//countdown, "Expired", Copy, several codes in the model's order, and closing
//consumes them.
TestCase {
    id: tc
    name: "AdminCodeSheet"
    when: windowShown
    visible: true
    width: 420
    height: 900

    QtObject {
        id: calaosApp
        property int applicationStatus: 0
    }

    QtObject {
        id: model
        property var codes: []
        property bool unreadable: false
        readonly property int lifetime: 60
        property var copied: []
        property int closes: 0
        property var sheetVisible: []
        signal showRequested()
        function copy(id) { copied = copied.concat([id]) }
        function close() { closes++; codes = []; unreadable = false }
        function setSheetVisible(v) { sheetVisible = sheetVisible.concat([v]) }
    }

    AdminCodeSheet {
        id: host
        codeModel: model
    }

    function code(id, digits, remaining, expired) {
        return { codeId: id, action: "console_open", code: expired ? "" : digits,
                 expired: expired === true, remaining: remaining, fraction: remaining / 60 }
    }

    function find(item, name) {
        if (item === null || item === undefined)
            return null
        if (item.objectName === name)
            return item
        var kids = item.children
        for (var i = 0; i < kids.length; i++) {
            var r = find(kids[i], name)
            if (r)
                return r
        }
        return null
    }

    function digitsOf(entry) {
        var row = find(entry, "adminCodeDigits")
        var s = ""
        for (var i = 0; i < row.children.length; i++)
            if (row.children[i].text !== undefined)
                s += row.children[i].text
        return s
    }

    function showCodes(list) {
        model.codes = list
        model.showRequested()
        tryVerify(function() { return host.opened }, 3000)
        wait(300)
    }


    function init() {
        model.codes = []
        model.unreadable = false
        model.copied = []
        model.closes = 0
        model.sheetVisible = []
    }

    function cleanup() {
        var close = find(Overlay.overlay, "adminCodeClose")
        if (host.opened && close)
            mouseClick(close)
        //Fully closed before the next case opens it again.
        tryVerify(function() { return !host.opened && !host.shown }, 3000)
        wait(50)
    }

    function test_opensOnlyWhenAsked() {
        model.codes = [code("0123456789abcdef", "482913", 60)]
        wait(100)
        verify(!host.opened)
        model.showRequested()
        tryVerify(function() { return host.opened }, 3000)
    }

    function test_digitsAndCountdown() {
        showCodes([code("0123456789abcdef", "482913", 60)])
        var entry = find(Overlay.overlay, "adminCode_0123456789abcdef")
        verify(entry)
        compare(digitsOf(entry), "482913")
        compare(find(entry, "adminCodePurpose").text, "To open the web console")
        compare(find(entry, "adminCodeCountdown").text, "Valid for 60 s")
        verify(!find(entry, "adminCodeExpired").visible)

        model.codes = [code("0123456789abcdef", "482913", 42)]
        entry = find(Overlay.overlay, "adminCode_0123456789abcdef")
        compare(find(entry, "adminCodeCountdown").text, "Valid for 42 s")
        //The bar drains towards 42/60 of the track.
        var bar = find(entry, "adminCodeBar")
        tryVerify(function() { return Math.abs(bar.width - bar.parent.width * 42 / 60) < 1 }, 3000)
    }

    function test_expired() {
        showCodes([code("0123456789abcdef", "482913", 3)])
        model.codes = [code("0123456789abcdef", "", 0, true)]
        var entry = find(Overlay.overlay, "adminCode_0123456789abcdef")
        verify(find(entry, "adminCodeExpired").visible)
        compare(find(entry, "adminCodeExpired").text, "Expired")
        verify(!find(entry, "adminCodeDigits").visible)
        //Nothing left to copy.
        verify(!find(entry, "adminCodeCopy").visible)
        //Still shown until closed.
        verify(host.opened)
    }

    function test_copy() {
        showCodes([code("0123456789abcdef", "482913", 50)])
        var copy = find(Overlay.overlay, "adminCodeCopy")
        verify(copy)
        mouseClick(copy)
        compare(model.copied, ["0123456789abcdef"])
    }

    function test_severalCodesNewestFirst() {
        showCodes([code("fedcba9876543210", "007351", 55), code("0123456789abcdef", "482913", 40)])
        var newest = find(Overlay.overlay, "adminCode_fedcba9876543210")
        var oldest = find(Overlay.overlay, "adminCode_0123456789abcdef")
        verify(newest && oldest)
        verify(newest.y < oldest.y)
        compare(digitsOf(newest), "007351")
        compare(digitsOf(oldest), "482913")

        //Each Copy copies its own code.
        mouseClick(find(oldest, "adminCodeCopy"))
        mouseClick(find(newest, "adminCodeCopy"))
        compare(model.copied, ["0123456789abcdef", "fedcba9876543210"])
    }

    function test_closeConsumes() {
        showCodes([code("0123456789abcdef", "482913", 50)])
        var close = find(Overlay.overlay, "adminCodeClose")
        verify(close)
        mouseClick(close)
        //closed() comes at the end of the exit transition.
        tryCompare(model, "closes", 1, 3000)
        compare(model.codes.length, 0)
        //Secure screen on while shown, off once closed.
        compare(model.sheetVisible, [true, false])
    }

    function test_unreadableHint() {
        showCodes([code("0123456789abcdef", "482913", 50)])
        var hint = find(Overlay.overlay, "adminCodeUnreadable")
        verify(hint)
        verify(!hint.visible)
        model.unreadable = true
        verify(hint.visible)
        compare(hint.text, "A code arrived but could not be read on this phone.")
    }

    function test_unreadableAlone() {
        //Nothing readable: the sheet still tells the approver.
        model.unreadable = true
        showCodes([])
        verify(find(Overlay.overlay, "adminCodeUnreadable").visible)
        compare(find(Overlay.overlay, "adminCode_0123456789abcdef"), null)
    }

    function test_purposes() {
        compare(host.purpose("admin_password"), "To change the admin password")
        compare(host.purpose("approver_enroll"), "To add an approver phone")
        compare(host.purpose("approver_remove"), "To remove an approver phone")
        compare(host.purpose("approver_confirm"), "To enroll this phone as the admin approver")
        compare(host.purpose("network_config"), "To change the network configuration")
        compare(host.purpose("unknown"), "")
    }
}
