import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import "../../qml/mobile"

//The versions page of the phone: what the box gave is listed, what it could
//not give is said in a sentence, and the server line stays in every case.
//With SETTINGS_SHOTS_DIR set the states are also written as images.
TestCase {
    id: tc
    name: "SettingsVersions"
    when: windowShown
    visible: true
    width: 420
    height: 900

    property bool isLandscape: false
    readonly property string shotsDir: harness.env("SETTINGS_SHOTS_DIR")

    QtObject {
        id: versionsModel
        property bool loading: false
        property bool loaded: false
        property string clientName: "calaos-home android"
        property string clientVersion: "4.5.0"
        property string qtVersion: "6.12.0"
        property string serverVersion: ""
        property bool systemAvailable: false
        property string containerVersion: ""
        property string calaosOs: ""
        property string debian: ""
        property string kernel: ""
        property var packages: []
        property string failure: ""
        property string message: ""
        property int refreshes: 0
        property int copies: 0
        function refresh() { refreshes++; loading = true }
        function copyToClipboard() { copies++ }
    }
    QtObject {
        id: calaosApp
        function getPictureSized(pic) { return "" }
    }

    Rectangle { anchors.fill: parent; color: Theme.backgroundColor }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
    }

    Component {
        id: pageComponent
        SettingsVersionsView {}
    }

    function init() {
        versionsModel.loading = false
        versionsModel.loaded = false
        versionsModel.serverVersion = ""
        versionsModel.systemAvailable = false
        versionsModel.containerVersion = ""
        versionsModel.calaosOs = ""
        versionsModel.debian = ""
        versionsModel.kernel = ""
        versionsModel.packages = []
        versionsModel.failure = ""
        versionsModel.message = ""
        versionsModel.refreshes = 0
        versionsModel.copies = 0
    }

    function cleanup() {
        while (stack.depth > 1)
            stack.pop(null, StackView.Immediate)
        wait(0)
    }

    function openPage() {
        var page = stack.push(pageComponent, {}, StackView.Immediate)
        verify(page)
        return page
    }

    function shot(file) {
        if (shotsDir === "")
            return
        wait(50)
        grabImage(tc).save(shotsDir + "/" + file)
    }

    function nominal() {
        versionsModel.serverVersion = "4.8.0"
        versionsModel.systemAvailable = true
        versionsModel.containerVersion = "1.9.1"
        versionsModel.calaosOs = "v4.4.0"
        versionsModel.debian = "13.1"
        versionsModel.kernel = "6.12.48"
        versionsModel.packages = [{ name: "calaos-container", version: "1.9.1" },
                                  { name: "calaos-server", version: "4.8.0" },
                                  { name: "calaos-web-app", version: "4.2.0" }]
        versionsModel.loading = false
        versionsModel.loaded = true
    }

    function failed(failure, message) {
        versionsModel.serverVersion = "4.8.0"
        versionsModel.systemAvailable = false
        versionsModel.failure = failure
        versionsModel.message = message
        versionsModel.loading = false
        versionsModel.loaded = true
    }

    function shown(page, name) {
        var item = findChild(page, name)
        verify(item, name)
        return item.visible
    }

    function test_openingReadsTheVersions() {
        openPage()
        compare(versionsModel.refreshes, 1)
    }

    function test_loadingSaysSoAndCannotBeCopied() {
        var page = openPage()
        verify(shown(page, "stateLoading"))
        verify(!shown(page, "serverLine"))
        verify(findChild(page, "copyButton").disabled)
        verify(findChild(page, "reloadButton").disabled)
        shot("versions-loading.png")
    }

    function test_clientLineNamesTheAppAndQt() {
        var page = openPage()
        var line = findChild(page, "clientLine")
        compare(line.name, "calaos-home android")
        compare(line.version, "4.5.0 (Qt 6.12.0)")
    }

    function test_nominalListsTheBoxAndItsPackages() {
        var page = openPage()
        nominal()
        verify(!shown(page, "stateLoading"))
        // the Calaos components appear once, in the package list
        verify(!shown(page, "serverLine"))
        verify(!shown(page, "stateMessage"))
        compare(findChild(page, "packageRepeater").count, 3)
        verify(!findChild(page, "copyButton").disabled)
        shot("versions-nominal.png")
    }

    function test_withoutTheSystemTheServerStaysAndTheReasonIsSaid_data() {
        return [
            { tag: "unreachable", failure: "unreachable", message: "The box system did not answer" },
            { tag: "daemon too old", failure: "daemon_too_old", message: "update calaos-container" },
            { tag: "old server", failure: "old_server", message: "update calaos-server" },
        ]
    }

    function test_withoutTheSystemTheServerStaysAndTheReasonIsSaid(data) {
        var page = openPage()
        failed(data.failure, data.message)
        verify(shown(page, "serverLine"))
        verify(shown(page, "stateMessage"))
        compare(findChild(page, "stateMessage").text, data.message)
        compare(findChild(page, "packageRepeater").count, 0)
        shot("versions-" + data.failure + ".png")
    }

    function test_unknownServerVersionIsSaid() {
        var page = openPage()
        failed("old_server", "update calaos-server")
        versionsModel.serverVersion = ""
        compare(findChild(page, "serverLine").version, "unknown")
    }

    function test_reloadAsksAgain() {
        var page = openPage()
        nominal()
        mouseClick(findChild(page, "reloadButton"))
        compare(versionsModel.refreshes, 2)
    }

    function test_copyCopiesAndSaysSo() {
        var page = openPage()
        nominal()
        verify(!shown(page, "copiedText"))
        mouseClick(findChild(page, "copyButton"))
        compare(versionsModel.copies, 1)
        verify(shown(page, "copiedText"))
        shot("versions-copied.png")
    }
}
