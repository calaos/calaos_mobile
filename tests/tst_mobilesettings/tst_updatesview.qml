import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
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
        id: hardwareUtils
        property bool nativeOn: false
        property var requests: []
        signal nativePasswordEntered(string purpose, string user, string password)
        signal nativePasswordCanceled(string purpose)
        function hasNativePasswordEntry() { return nativeOn }
        function requestNativePassword(purpose, options) {
            requests.push({ purpose: purpose, options: options })
            return true
        }
    }
    QtObject {
        id: calaosApp
        property int logouts: 0
        signal applicationStatusChanged(int status)
        function getPictureSized(pic) { return "" }
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
        hardwareUtils.nativeOn = false
        hardwareUtils.requests = []
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

    //iOS/Android: the password is typed in the platform's own dialog, the
    //only fields the password manager fills.
    function test_nativeEntryReplacesTheField() {
        hardwareUtils.nativeOn = true
        var page = openPage()
        verify(!findChild(page, "adminPasswordField").visible)
        var button = findChild(page, "unlockButton")
        verify(!button.disabled)
        mouseClick(button)
        compare(hardwareUtils.requests.length, 1)
        compare(hardwareUtils.requests[0].purpose, "admin_unlock")
        compare(hardwareUtils.requests[0].options.user, "admin")

        //another request's answer is not ours
        hardwareUtils.nativePasswordEntered("login", "user", "household")
        compare(boxUpdates.unlocked.length, 0)
        hardwareUtils.nativePasswordEntered("admin_unlock", "admin", "admin pass")
        compare(boxUpdates.unlocked.length, 1)
        compare(boxUpdates.unlocked[0], "admin pass")
    }

    function test_nativeAnswerIgnoredOnceTheGateMoved() {
        hardwareUtils.nativeOn = true
        openPage()
        boxUpdates.gate = "idle" //socket lost while the dialog was open
        hardwareUtils.nativePasswordEntered("admin_unlock", "admin", "late")
        compare(boxUpdates.unlocked.length, 0)
    }

    function test_nativeEntryWaitsForRateLimit() {
        hardwareUtils.nativeOn = true
        boxUpdates.retryAfter = 10
        var page = openPage()
        verify(findChild(page, "unlockButton").disabled)
        page.askNativePassword()
        compare(hardwareUtils.requests.length, 0)
    }
}
