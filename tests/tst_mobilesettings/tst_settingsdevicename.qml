import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import "../../qml/mobile"

//The device name field of the mobile settings: only what the user typed is
//saved, on every way out of the page; an untouched field follows a rename
//of the phone.
TestCase {
    id: tc
    name: "SettingsDeviceName"
    when: windowShown
    visible: true
    width: 420
    height: 900

    property bool isLandscape: false

    //Stand-ins for the context properties the page reads, found by its
    //unqualified lookups before the root context.
    QtObject {
        id: hardwareUtils
        property string platformName: "iPhone"
        property string override: ""
        readonly property string deviceName: override !== "" ? override : platformName
        readonly property int deviceNameMaxLength: 64
        property var saved: []
        function setDeviceNameOverride(name) {
            saved.push(name)
            var n = name.replace(/\s+/g, " ").trim()
            override = n === platformName ? "" : n
        }
    }
    QtObject {
        id: calaosApp
        function getPictureSized(pic) { return "" }
    }
    ListModel { id: langModel }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
    }

    Component {
        id: settingsComponent
        SettingsDeviceView {}
    }

    function init() {
        hardwareUtils.platformName = "iPhone"
        hardwareUtils.override = ""
        hardwareUtils.saved = []
    }

    function cleanup() {
        while (stack.depth > 1)
            stack.pop(null, StackView.Immediate)
        wait(0)
    }

    function openPage() {
        var page = stack.push(settingsComponent, {}, StackView.Immediate)
        verify(page)
        var field = findChild(page, "deviceNameField")
        verify(field)
        compare(field.text, hardwareUtils.deviceName)
        return { page: page, field: field }
    }

    function leave() {
        stack.pop(null, StackView.Immediate)
        wait(0)
    }

    function type(field, text) {
        field.forceActiveFocus()
        tryVerify(function() { return field.activeFocus })
        field.selectAll()
        for (var i = 0; i < text.length; i++)
            keyClick(text[i])
    }

    function test_untouchedFieldFollowsASystemRename() {
        var p = openPage()
        hardwareUtils.platformName = "iPhone 15"
        compare(p.field.text, "iPhone 15")
        leave()
        compare(hardwareUtils.saved.length, 0)
        compare(hardwareUtils.override, "")
    }

    function test_focusedButUntouchedSavesNothing() {
        var p = openPage()
        p.field.forceActiveFocus()
        tryVerify(function() { return p.field.activeFocus })
        leave()
        compare(hardwareUtils.saved.length, 0)
    }

    function test_typedNameIsSavedWhenLeavingWithTheKeyboardOpen() {
        var p = openPage()
        type(p.field, "Pixel")
        verify(p.field.activeFocus)
        leave()
        compare(hardwareUtils.saved, ["Pixel"])
        compare(hardwareUtils.deviceName, "Pixel")
    }

    function test_doneSavesOnceAndShowsTheStoredName() {
        var p = openPage()
        type(p.field, "a  b")
        keyClick(Qt.Key_Return)
        tryVerify(function() { return !p.field.activeFocus })
        compare(hardwareUtils.saved, ["a  b"])
        compare(p.field.text, "a b")
        leave()
        compare(hardwareUtils.saved.length, 1)
    }

    function test_saveWhileTypingLeavesTheFieldAlone() {
        //What the app going to the background does with the keyboard open.
        var p = openPage()
        type(p.field, "  Ab")
        p.page.commitDeviceName()
        compare(hardwareUtils.saved, ["  Ab"])
        compare(p.field.text, "  Ab")
        verify(p.field.activeFocus)
        //Nothing new typed: leaving saves nothing more, the field is rebound.
        p.field.focus = false
        compare(p.field.text, "Ab")
        leave()
        compare(hardwareUtils.saved.length, 1)
    }

    function test_renameWhileTypingKeepsWhatIsTyped() {
        var p = openPage()
        type(p.field, "Pix")
        hardwareUtils.platformName = "iPhone 15"
        compare(p.field.text, "Pix")
        leave()
        compare(hardwareUtils.saved, ["Pix"])
    }

    Component {
        id: plainFieldComponent
        TextField { width: 200 }
    }
    Component {
        id: calaosFieldComponent
        CalaosTextField { width: 200; text: "a long text" }
    }

    //LoginView uses the unframed field: it keeps the style padding and a
    //clear zone the size of the icon, so a tap at the end of the text
    //cannot empty it.
    function test_unframedFieldKeepsItsStyleAndItsClearZone() {
        var plain = createTemporaryObject(plainFieldComponent, tc)
        var f = createTemporaryObject(calaosFieldComponent, tc)
        verify(plain && f)
        compare(f.leftPadding, plain.leftPadding)
        compare(f.rightPadding, plain.rightPadding)
        var area = findChild(f, "clearArea")
        compare(area.width, area.parent.width)
        compare(area.x, 0)
        f.framed = true
        compare(f.leftPadding, 12)
        compare(area.width, area.parent.width + 26)
        f.framed = false
        compare(f.leftPadding, plain.leftPadding)
        compare(area.width, area.parent.width)
    }

    function test_clearButtonThenLeaveRevertsToTheDefault() {
        hardwareUtils.override = "Pixel"
        var p = openPage()
        compare(p.field.text, "Pixel")
        p.field.forceActiveFocus()
        tryVerify(function() { return p.field.activeFocus })
        //What the clear icon of CalaosTextField does, in that order.
        p.field.clear()
        p.field.clearButtonClicked()
        leave()
        compare(hardwareUtils.saved, [""])
        compare(hardwareUtils.override, "")
    }
}
