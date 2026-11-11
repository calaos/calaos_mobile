import QtQuick
import QtTest
import SharedComponents
import "../../qml/desktop"

//The versions page of the kiosk on the real VersionsModel: what it shows for
//each answer of the box, the Copy and Reload buttons, and the "All versions"
//button of the system info panel that opens it.
TestCase {
    id: tc
    name: "ConfigVersionsView"
    when: windowShown
    visible: true
    width: 1280
    height: 800

    readonly property string shotsDir: harness.env("VERSIONS_SHOTS_DIR")

    Rectangle { anchors.fill: parent; color: "#272727" }

    Item {
        id: rootWindow
        anchors.fill: parent
        property int backCalls: 0
        property int desktopCalls: 0
        property var clicked: []
        function handleBack() { backCalls++ }
        function goToDesktop() { desktopCalls++ }
        function handleSubitemClick(id) { clicked.push(id) }
    }

    QtObject {
        id: calaosApp
        property string appVersion: "4.5.1"
        property int uptime: 93784
        property string machineName: "calaos-box"
        property var netAddresses: []
        property int cpuUsage: 12
        property int memoryUsage: 41
        function updateSystemInfo() {}
        function updateNetworkInfo() {}
        function getPictureSized(p) { return "" }
    }

    Component { id: pageComponent; ConfigVersionsView { anchors.fill: parent } }
    Component { id: tabsComponent; ConfigTabs { width: 300; height: 700; x: 940; y: 20 } }

    function init() {
        harness.reset()
        rootWindow.backCalls = 0
        rootWindow.desktopCalls = 0
        rootWindow.clicked = []
        harness.setClipboardText("untouched")
    }

    function open() {
        var page = createTemporaryObject(pageComponent, rootWindow)
        verify(page)
        waitForRendering(page)
        return page
    }

    function shot(name) {
        if (shotsDir !== "")
            grabImage(tc).save(shotsDir + "/versions-" + name + ".png")
    }

    //Texts shown in a subtree (an invisible item hides its whole subtree)
    function texts(item, out) {
        out = out || []
        if (!item.visible)
            return out
        if (typeof item.text === "string" && item.text !== "")
            out.push(item.text)
        var kids = item.children
        for (var i = 0; i < kids.length; i++)
            texts(kids[i], out)
        if (item.contentItem)
            texts(item.contentItem, out)
        return out
    }

    function rowText(row) {
        return texts(row).join(" ")
    }

    function hasText(page, s) {
        return texts(page).indexOf(s) >= 0
    }

    function footerButton(page, label) {
        var found = null
        function walk(it) {
            if (found)
                return
            if (typeof it.label === "string" && it.label === label)
                found = it
            for (var i = 0; i < it.children.length; i++)
                walk(it.children[i])
            if (it.contentItem)
                walk(it.contentItem)
        }
        walk(page)
        return found
    }

    function test_loadsWhenOpened() {
        compare(harness.fetchCount(), 0)
        open()
        compare(harness.fetchCount(), 1)
    }

    function test_loading() {
        var page = open()
        verify(versionsModel.loading)
        verify(findChild(page, "stateLoading").visible)
        verify(!findChild(page, "serverRow").visible)
        verify(!findChild(page, "containerRow").visible)
        verify(!findChild(page, "stateMessage").visible)
        // the versions of this screen need no answer
        verify(findChild(page, "clientRow").visible)
        compare(rowText(findChild(page, "clientRow")), "This screen 4.5.1 (Qt " + versionsModel.qtVersion + ")")
        verify(findChild(page, "copyButton").disabled)
        shot("loading")
    }

    function test_nominal() {
        var page = open()
        harness.answerDaemon(200, "4.8.0")
        verify(!findChild(page, "stateLoading").visible)
        compare(rowText(findChild(page, "serverRow")), "calaos-server 4.8.0")
        compare(rowText(findChild(page, "containerRow")), "calaos-container 1.9.1")
        verify(hasText(page, "Calaos OS") && hasText(page, "v4.4.0"))
        verify(hasText(page, "Debian") && hasText(page, "13.1"))
        verify(hasText(page, "Kernel") && hasText(page, "6.12.48-amd64"))
        verify(hasText(page, "Packages"))
        verify(hasText(page, "calaos-web-app") && hasText(page, "4.2.0"))
        verify(!findChild(page, "stateMessage").visible)
        verify(!findChild(page, "copyButton").disabled)
        shot("nominal")
    }

    function test_serverVersionUnknown() {
        var page = open()
        harness.answerDaemon(200, "")
        compare(rowText(findChild(page, "serverRow")), "calaos-server unknown")
    }

    function failureCase(answer, server, text) {
        var page = open()
        answer()
        verify(!findChild(page, "stateLoading").visible)
        var msg = findChild(page, "stateMessage")
        verify(msg.visible)
        verify(msg.text.indexOf(text) >= 0, msg.text)
        // the server version is still given, nothing of the system is
        compare(rowText(findChild(page, "serverRow")), "calaos-server " + server)
        verify(!findChild(page, "containerRow").visible)
        verify(!hasText(page, "Packages"))
        verify(!hasText(page, "Debian"))
        verify(!hasText(page, "Kernel"))
        verify(!hasText(page, "Calaos OS"))
        return page
    }

    function test_oldServer_data() {
        return [ { tag: "not_allowed", code: "not_allowed" },
                 { tag: "old_server", code: "old_server" },
                 { tag: "invalid_request", code: "invalid_request" } ]
    }

    function test_oldServer(data) {
        var page = failureCase(function() { harness.answerRelayError(data.code, "4.8.0") },
                               "4.8.0", "update calaos-server")
        verify(Qt.colorEqual(findChild(page, "stateMessage").color, Theme.blueColor))
        shot("old_server")
    }

    function test_daemonTooOld() {
        var page = failureCase(function() { harness.answerDaemon(404, "4.8.0") },
                               "4.8.0", "update calaos-container")
        verify(Qt.colorEqual(findChild(page, "stateMessage").color, Theme.blueColor))
        shot("daemon_too_old")
    }

    function test_unreachable_data() {
        return [ { tag: "transport error", status: 0 },
                 { tag: "server error", status: 500 } ]
    }

    function test_unreachable(data) {
        var page = failureCase(function() { harness.answerDaemon(data.status, "4.8.0") },
                               "4.8.0", "did not answer")
        verify(Qt.colorEqual(findChild(page, "stateMessage").color, Theme.redColor))
        shot("unreachable")
    }

    //A refused token is not a box that does not answer
    function test_unauthorized() {
        var page = failureCase(function() { harness.answerDaemon(401, "4.8.0") },
                               "4.8.0", "authentication error")
        var msg = findChild(page, "stateMessage")
        verify(msg.text.indexOf("did not answer") < 0)
        verify(Qt.colorEqual(msg.color, Theme.redColor))
        shot("unauthorized")
    }

    function test_failureClearsOnNextGoodAnswer() {
        var page = open()
        harness.answerDaemon(401, "4.8.0")
        verify(findChild(page, "stateMessage").visible)
        mouseClick(findChild(page, "refresh"))
        harness.answerDaemon(200, "4.8.0")
        verify(!findChild(page, "stateMessage").visible)
        verify(findChild(page, "containerRow").visible)
    }

    function test_reloadKeepsTheShownValues() {
        var page = open()
        harness.answerDaemon(200, "4.8.0")
        mouseClick(findChild(page, "refresh"))
        compare(harness.fetchCount(), 2)
        verify(versionsModel.loading)
        // no flash back to "Reading": the previous values stay until the answer
        verify(!findChild(page, "stateLoading").visible)
        verify(findChild(page, "serverRow").visible)
        verify(!findChild(page, "copyButton").disabled)
        harness.answerDaemon(200, "4.8.1")
        compare(rowText(findChild(page, "serverRow")), "calaos-server 4.8.1")
    }

    function test_copy() {
        var page = open()
        harness.answerDaemon(200, "4.8.0")
        compare(findChild(page, "copiedText").text, "")
        mouseClick(findChild(page, "copyButton"))
        var copied = harness.clipboardText()
        verify(copied.indexOf("calaos-home kiosk") >= 0, copied)
        verify(copied.indexOf("4.5.1") >= 0)
        verify(copied.indexOf("calaos-server") >= 0 && copied.indexOf("4.8.0") >= 0)
        verify(copied.indexOf("calaos-container") >= 0 && copied.indexOf("1.9.1") >= 0)
        compare(findChild(page, "copiedText").text, "Copied to the clipboard")
        shot("copied")
    }

    function test_copyInAFailureCarriesTheError() {
        var page = open()
        harness.answerDaemon(401, "4.8.0")
        mouseClick(findChild(page, "copyButton"))
        var copied = harness.clipboardText()
        verify(copied.indexOf("4.8.0") >= 0, copied)
        verify(copied.indexOf("authentication error") >= 0, copied)
    }

    //Nothing to copy before the first answer
    function test_copyIsRefusedBeforeTheFirstAnswer() {
        var page = open()
        mouseClick(findChild(page, "copyButton"))
        compare(harness.clipboardText(), "untouched")
        compare(findChild(page, "copiedText").text, "")
    }

    function test_footerButtons() {
        var page = open()
        mouseClick(footerButton(page, "Back to config"))
        compare(rootWindow.backCalls, 1)
        mouseClick(footerButton(page, "Quit"))
        compare(rootWindow.desktopCalls, 1)
    }

    function test_allVersionsButtonOpensThePage() {
        var tabs = createTemporaryObject(tabsComponent, rootWindow)
        verify(tabs)
        waitForRendering(tabs)
        var b = findChild(tabs, "versionsButton")
        verify(b.visible)
        compare(b.text, "All versions")
        // inside the panel, not cut by it
        var tl = b.mapToItem(tabs, 0, 0)
        verify(tl.x >= 0 && tl.x + b.width <= tabs.width, "x=" + tl.x + " w=" + b.width)
        verify(tl.y >= 0 && tl.y + b.height <= tabs.height)
        mouseClick(b)
        compare(rootWindow.clicked.length, 1)
        compare(rootWindow.clicked[0], "config/versions")
        shot("all-versions-button")
    }

    function test_showVersionsHidesTheButton() {
        var tabs = createTemporaryObject(tabsComponent, rootWindow)
        verify(tabs)
        waitForRendering(tabs)
        var b = findChild(tabs, "versionsButton")
        tabs.showVersions = false
        verify(!b.visible)
        mouseClick(b)
        compare(rootWindow.clicked.length, 0)
        tabs.showVersions = true
        verify(b.visible)
    }

    //The page does not offer a link to itself
    function test_noLinkToItself() {
        var page = open()
        var b = findChild(page, "versionsButton")
        verify(b)
        verify(!b.visible)
    }

    function test_reachableFromTheKiosk() {
        verify(harness.isResource("qml/desktop/ConfigVersionsView.qml"))
    }

    //main.qml cannot be loaded in a test: its routing is read from the source
    function test_mainRoutesTheEntryToThePage() {
        var main = harness.resourceText("qml/desktop/main.qml")
        verify(main.length > 0)
        verify(/"config\/versions"\)\s*\{\s*item = configVersionsView\s*\}/.test(main))
        verify(/id: configVersionsView\s*ConfigVersionsView\s*\{/.test(main))
    }
}
