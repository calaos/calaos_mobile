import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import Calaos
import "../../qml/mobile"

//The box updates page: the admin password field never keeps or predicts
//what is typed, the page asks boxUpdates for the right thing at the right
//time, and a reboot always takes a confirmation.
TestCase {
    id: tc
    name: "UpdatesView"
    when: windowShown
    visible: true
    width: 420
    height: 900

    property bool isLandscape: false

    //Stand-ins for the context properties UpdatesView reads.
    QtObject {
        id: adminBiometrics
        property bool available: false
        property bool stored: false
        property string keepText: "Unlock with Face ID next time"
        property string turnOffText: "Turn off Face ID for this box"
        property var kept: []
        property int disabled: 0
        function unlockAndKeep(password) { kept.push(password) }
        function disable() { disabled++ }
    }
    QtObject {
        id: boxUpdates
        property string gate: "locked"
        property string phase: "unknown"
        property string busy: ""
        property int updatesCount: 0
        property int totalPercent: 0
        property string currentPackage: ""
        property int pkgIndex: 0
        property int pkgCount: 0
        property bool externalRun: false
        property string failureText: ""
        property string errorText: ""
        property int retryAfter: 0
        property string info: ""
        property bool rebootHint: false
        property string boxName: ""
        property var packagesModel: ListModel {}
        property var groupsModel: ListModel {}

        property int activations: 0
        property int deactivations: 0
        property int reboots: 0
        property int retries: 0
        property var unlocked: []
        function activate() { activations++ }
        function deactivate() { deactivations++ }
        function unlock(p) { unlocked.push(p) }
        function checkUpdates() {}
        function upgradeAll() {}
        function reboot() { reboots++ }
        function retry() { retries++ }
    }
    QtObject {
        id: calaosApp
        property int logouts: 0
        property bool isAndroid: false
        property string username: "raoul"
        property string password: ""
        property string hostname: "calaos.local"
        property int applicationStatus: 0
        function getPictureSized(pic) { return "qrc:/img/" + pic + ".png" }
        function userLogout() { logouts++ }
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
    }

    Component {
        id: updatesComponent
        UpdatesView {}
    }

    Component {
        id: otherPage
        Item {}
    }

    TextInput { id: clipboardSource; visible: false }

    function init() {
        boxUpdates.gate = "locked"
        boxUpdates.phase = "unknown"
        boxUpdates.busy = ""
        boxUpdates.retryAfter = 0
        boxUpdates.totalPercent = 0
        boxUpdates.activations = 0
        boxUpdates.deactivations = 0
        boxUpdates.reboots = 0
        boxUpdates.retries = 0
        boxUpdates.unlocked = []
        calaosApp.logouts = 0
        adminBiometrics.available = false
        adminBiometrics.stored = false
        adminBiometrics.kept = []
    }

    function cleanup() {
        while (stack.depth > 1)
            stack.pop(null, StackView.Immediate)
        wait(0)
    }

    function openPage() {
        var page = stack.push(updatesComponent, {}, StackView.Immediate)
        verify(page)
        wait(0)
        return page
    }

    function test_activatedAndDeactivated() {
        openPage()
        compare(boxUpdates.activations, 1)
        stack.pop(null, StackView.Immediate)
        wait(0)
        verify(boxUpdates.deactivations >= 1)
    }

    //SR-21
    function test_passwordFieldIsSensitive() {
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        verify(field)
        verify(field.visible)
        compare(field.echoMode, TextInput.Password)
        verify(field.inputMethodHints & Qt.ImhSensitiveData)
        verify(field.inputMethodHints & Qt.ImhNoPredictiveText)
        verify(field.inputMethodHints & Qt.ImhNoAutoUppercase)
        compare(field.text, "")
    }

    function test_unlockSendsAndClears() {
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        var button = findChild(page, "unlockButton")
        verify(button.disabled)
        field.text = "a long admin password"
        verify(!button.disabled)
        mouseClick(button)
        compare(boxUpdates.unlocked.length, 1)
        compare(boxUpdates.unlocked[0], "a long admin password")
        compare(field.text, "")
    }

    function test_unlockWaitsForRateLimit() {
        boxUpdates.retryAfter = 10
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        field.text = "again"
        field.accepted()
        compare(boxUpdates.unlocked.length, 0)
        verify(findChild(page, "unlockButton").disabled)
    }

    function test_leavingClearsThePassword() {
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        field.text = "typed but not sent"
        //another page on top: this one stays alive, deactivated
        stack.push(otherPage, {}, StackView.Immediate)
        wait(0)
        compare(field.text, "")
        verify(boxUpdates.deactivations >= 1)
        compare(boxUpdates.unlocked.length, 0)
    }

    function test_httpsRequiredOffersTheHostSetting() {
        boxUpdates.gate = "https_required"
        var page = openPage()
        verify(findChild(page, "updatesGateCard").visible)
        verify(!findChild(page, "updatesUnlockCard").visible)
        compare(findChild(page, "updatesGateTitle").text, "Updates need an https connection to the box")
        mouseClick(findChild(page, "updatesChangeHost"))
        compare(calaosApp.logouts, 1)
    }

    function test_adminNotSetPointsToTheWebapp() {
        boxUpdates.gate = "admin_not_set"
        var page = openPage()
        compare(findChild(page, "updatesGateTitle").text, "Set the admin password from the web app first")
        mouseClick(findChild(page, "updatesRetry"))
        compare(boxUpdates.retries, 1)
    }

    function test_rebootNeedsConfirmation() {
        boxUpdates.gate = "unlocked"
        boxUpdates.phase = "doneReboot"
        var page = openPage()
        var reboot = findChild(page, "updatesReboot")
        verify(reboot.visible)
        mouseClick(reboot)
        compare(boxUpdates.reboots, 0)
        var sheet = findChild(page, "rebootSheet")
        tryCompare(sheet, "opened", true)
        var confirm = findChild(sheet.contentItem, "rebootConfirm")
        verify(confirm)
        mouseClick(confirm)
        compare(boxUpdates.reboots, 1)
        tryCompare(sheet, "opened", false)
    }

    function test_householdProgressWhileLocked() {
        boxUpdates.gate = "locked"
        boxUpdates.phase = "upgrading"
        boxUpdates.totalPercent = 42
        var page = openPage()
        verify(findChild(page, "updatesCard").visible)
        verify(findChild(page, "updatesProgressBar").visible)
        compare(findChild(page, "updatesHeadline").text, "Updating… 42 %")
        //the unlock form stays offered
        verify(findChild(page, "updatesUnlockCard").visible)
    }

    function test_httpFallbackOffersToLogInAgain() {
        boxUpdates.gate = "websocket_required"
        var page = openPage()
        compare(findChild(page, "updatesGateTitle").text, "Updates need a WebSocket connection to the box")
        mouseClick(findChild(page, "updatesLoginAgain"))
        compare(calaosApp.logouts, 1)
    }

    function test_noRebootWhileUpgrading() {
        boxUpdates.gate = "unlocked"
        boxUpdates.phase = "upgrading"
        var page = openPage()
        verify(findChild(page, "updatesReboot").disabled)
        mouseClick(findChild(page, "updatesReboot"))
        verify(!findChild(page, "rebootSheet").opened)
    }

    function test_passwordClearedWhenTheGateMoves() {
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        field.text = "half typed"
        boxUpdates.gate = "idle" //socket lost, another box
        compare(field.text, "")
    }

    function test_restartCallToActionAfterAnUpdate() {
        boxUpdates.gate = "unlocked"
        boxUpdates.phase = "doneReboot"
        var page = openPage()
        var cta = findChild(page, "updatesRebootNow")
        verify(cta.visible)
        mouseClick(cta)
        var sheet = findChild(page, "rebootSheet")
        tryCompare(sheet, "opened", true)
        //the session ends: the question goes with it
        boxUpdates.gate = "locked"
        tryCompare(sheet, "opened", false)
        compare(boxUpdates.reboots, 0)
    }

    function test_rebootQuestionClosesWhenAnUpgradeStarts() {
        boxUpdates.gate = "unlocked"
        boxUpdates.phase = "upToDate"
        var page = openPage()
        mouseClick(findChild(page, "updatesReboot"))
        var sheet = findChild(page, "rebootSheet")
        tryCompare(sheet, "opened", true)
        boxUpdates.phase = "upgrading"
        tryCompare(sheet, "opened", false)
        verify(findChild(sheet.contentItem, "rebootConfirm").disabled)
        compare(boxUpdates.reboots, 0)
    }

    //Typed in place, without a dialog, and a copied password can be pasted.
    function test_passwordFieldInPlaceWithoutNativeEntry() {
        var page = openPage()
        var field = findChild(page, "adminPasswordField")
        verify(field.visible)
        compare(field.echoMode, TextInput.Password)
        verify(field.inputMethodHints & Qt.ImhSensitiveData)
        verify(field.inputMethodHints & Qt.ImhNoPredictiveText)
        verify(field.inputMethodHints & Qt.ImhNoAutoUppercase)

        clipboardSource.text = "copied secret"
        clipboardSource.selectAll()
        clipboardSource.copy()
        field.forceActiveFocus()
        field.paste()
        compare(field.text, "copied secret")

        mouseClick(findChild(page, "unlockButton"))
        compare(boxUpdates.unlocked.length, 1)
        compare(boxUpdates.unlocked[0], "copied secret")
        compare(field.text, "")
    }

    function test_capture() {
        var dir = harness.env("SETTINGS_SHOTS_DIR")
        if (dir === "")
            skip("SETTINGS_SHOTS_DIR not set")
        tc.width = 360
        boxUpdates.boxName = "calaos-box"
        var page = openPage()
        wait(50)
        grabImage(tc).save(dir + "/settings-box.png")

        //No keyboard offscreen: its place is forced and drawn.
        tc.height = 700
        findChild(page, "adminPasswordField").forceActiveFocus()
        var scroller = findScroller(page)
        verify(scroller)
        scroller.keyboardRect = Qt.rect(0, tc.height - 300, tc.width, 300)
        keyboardBlock.visible = true
        wait(300)
        grabImage(tc).save(dir + "/settings-box-keyboard.png")
        keyboardBlock.visible = false
        scroller.keyboardRect = Qt.rect(0, 0, 0, 0)
        tc.height = 900
        stack.pop(null, StackView.Immediate)

        boxUpdates.gate = "unlocked"
        boxUpdates.phase = "available"
        boxUpdates.updatesCount = 2
        boxUpdates.packagesModel.append({ pkgName: "calaos-server", pkgInstalledVersion: "3.2.0",
                                          pkgNewVersion: "3.3.0", pkgHasUpdate: true })
        boxUpdates.packagesModel.append({ pkgName: "calaos-container", pkgInstalledVersion: "1.4.1",
                                          pkgNewVersion: "1.5.0", pkgHasUpdate: true })
        openPage()
        wait(300)
        grabImage(tc).save(dir + "/settings-box-unlocked.png")
        boxUpdates.packagesModel.clear()
        boxUpdates.boxName = ""
        stack.pop(null, StackView.Immediate)

        //LoginView shares CalaosTextField with the settings, unframed
        stack.push(loginComponent, {}, StackView.Immediate)
        wait(300)
        grabImage(tc).save(dir + "/loginview.png")
        tc.width = 420
    }

    Component {
        id: loginComponent
        LoginView {}
    }

    function findScroller(item) {
        if (item.keyboardRect !== undefined)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var found = findScroller(item.children[i])
            if (found)
                return found
        }
        return null
    }

    Rectangle {
        id: keyboardBlock
        visible: false
        z: 10
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 300
        color: "#2b2b2b"
    }

    function test_keepPasswordFollowsTheBiometrics_data() {
        return [
            { tag: "available", available: true, stored: false, shown: true },
            { tag: "stored", available: true, stored: true, shown: false },
            { tag: "unavailable", available: false, stored: false, shown: false }
        ]
    }

    function test_keepPasswordFollowsTheBiometrics(data) {
        adminBiometrics.available = data.available
        adminBiometrics.stored = data.stored
        var page = openPage()
        var box = findChild(page, "keepPassword")
        compare(box.visible, data.shown)
        compare(box.checked, false)
    }

    function test_uncheckedKeepsNothing() {
        adminBiometrics.available = true
        var page = openPage()
        findChild(page, "adminPasswordField").text = "pass"
        mouseClick(findChild(page, "unlockButton"))
        compare(boxUpdates.unlocked.length, 1)
        compare(adminBiometrics.kept.length, 0)
    }

    function test_checkedKeepsThePassword() {
        adminBiometrics.available = true
        var page = openPage()
        waitForRendering(page)
        var box = findChild(page, "keepPassword")
        mouseClick(box, box.width / 2, box.height / 2)
        verify(box.checked)
        findChild(page, "adminPasswordField").text = "pass"
        mouseClick(findChild(page, "unlockButton"))
        compare(boxUpdates.unlocked.length, 0)
        compare(adminBiometrics.kept.length, 1)
        compare(adminBiometrics.kept[0], "pass")
    }
}
