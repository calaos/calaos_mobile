import QtQuick
import QtQuick.Controls
import QtTest
import QuickFlux
import SharedComponents
import "../../qml/mobile"

//The settings entry and its sub-pages: every setting is on its page and does
//what it did, each sub-page comes back to the entry.
//With SETTINGS_SHOTS_DIR set the pages are also written as images.
TestCase {
    id: tc
    name: "SettingsNavigation"
    when: windowShown
    visible: true
    width: 420
    height: 900

    property bool isLandscape: false
    readonly property string shotsDir: harness.env("SETTINGS_SHOTS_DIR")

    //Stand-ins for the context properties the pages read, found by their
    //unqualified lookups before the root context.
    QtObject {
        id: hardwareUtils
        property string override: ""
        readonly property string deviceName: override !== "" ? override : "iPhone"
        readonly property int deviceNameMaxLength: 64
        property var saved: []
        function setDeviceNameOverride(name) {
            saved.push(name)
            override = name
        }
    }
    QtObject {
        id: calaosApp
        property var tlsPinList: []
        property int logouts: 0
        property int resets: 0
        property var languages: []
        property var forgotten: []
        function getPictureSized(pic) { return "qrc:/img/" + pic + ".png" }
        function userLogout() { logouts++ }
        function resetAllData() { resets++; hardwareUtils.override = "" }
        function setLanguage(code) { languages.push(code) }
        function forgetTlsPin(host) { forgotten.push(host) }
    }
    ListModel {
        id: langModel
        function getLangModel(idx) { return get(idx) }
    }
    ListModel {
        id: eventLogModel
        ListElement { evDate: "1 November"; evTitle: "Light kitchen"; evTime: "18:42" }
        ListElement { evDate: "1 November"; evTitle: "Shutter living room"; evTime: "18:10" }
        function loadMore() {}
        function refresh() {}
    }

    SignalSpy { id: subPageSpy; signalName: "subPageOpened" }
    SignalSpy { id: addSpy; signalName: "favoriteAddClicked" }
    SignalSpy { id: editSpy; signalName: "favoriteEditClicked" }
    SignalSpy { id: boxSpy; signalName: "boxUpdatesClicked" }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: Item {}
    }

    Component {
        id: settingsComponent
        SettingsView {}
    }

    Component {
        id: eventLogComponent
        EventLogView {}
    }

    function init() {
        hardwareUtils.override = ""
        hardwareUtils.saved = []
        calaosApp.tlsPinList = []
        calaosApp.logouts = 0
        calaosApp.resets = 0
        calaosApp.languages = []
        calaosApp.forgotten = []
        langModel.clear()
        langModel.append({ langCode: "en", langName: "English", langActive: false })
        langModel.append({ langCode: "fr", langName: "Français", langActive: true })
        AppDispatcher.clear()
    }

    function cleanup() {
        stack.pop(null, StackView.Immediate)
        wait(0)
    }

    function openSettings() {
        var page = stack.push(settingsComponent, {}, StackView.Immediate)
        verify(page)
        subPageSpy.target = page
        addSpy.target = page
        editSpy.target = page
        boxSpy.target = page
        subPageSpy.clear()
        addSpy.clear()
        editSpy.clear()
        boxSpy.clear()
        return page
    }

    function settle() {
        tryVerify(function() { return !stack.busy })
        wait(0)
    }

    function clickOn(page, name) {
        var item = findChild(page, name)
        verify(item, name)
        mouseClick(item)
    }

    function openSubPage(entry) {
        var page = openSettings()
        clickOn(page, entry)
        settle()
        verify(stack.currentItem !== page, entry)
        return stack.currentItem
    }

    function back() {
        stack.pop()
        settle()
    }

    function shot(file) {
        if (shotsDir === "")
            return
        wait(50)
        grabImage(tc).save(shotsDir + "/" + file)
    }

    function test_entryListsTheFiveSubPagesInOrder() {
        var page = openSettings()
        var names = ["settingsEntryDevice", "settingsEntryConnection", "settingsEntryBox",
                     "settingsEntryFavorites", "settingsEntryEventLog"]
        var previous = -1
        for (var i = 0; i < names.length; i++) {
            var entry = findChild(page, names[i])
            verify(entry, names[i])
            verify(entry.visible, names[i])
            verify(entry.y > previous, names[i])
            previous = entry.y
        }
    }

    function test_subPageComesBackToTheEntry_data() {
        return [
            { tag: "device", entry: "settingsEntryDevice", title: "Device" },
            { tag: "connection", entry: "settingsEntryConnection", title: "Connection" },
            { tag: "favorites", entry: "settingsEntryFavorites", title: "Favorites" },
        ]
    }

    function test_subPageComesBackToTheEntry(data) {
        var entryPage = openSettings()
        for (var i = 0; i < 2; i++) {
            clickOn(entryPage, data.entry)
            settle()
            compare(stack.depth, 3)
            compare(stack.currentItem.title, data.title)
            back()
            compare(stack.depth, 2)
            compare(stack.currentItem, entryPage)
            verify(entryPage.visible)
        }
        //main.qml switches to the back button on this, without it the
        //sub-page has no way out.
        compare(subPageSpy.count, 2)
    }

    function test_boxEntryAsksForTheUpdatesPage() {
        var page = openSettings()
        clickOn(page, "settingsEntryBox")
        compare(boxSpy.count, 1)
        compare(stack.currentItem, page)
    }

    function test_eventLogEntryOpensTheLog() {
        var page = openSettings()
        clickOn(page, "settingsEntryEventLog")
        compare(AppDispatcher.log.length, 1)
        compare(stack.currentItem, page)
    }

    //The table of the settings: a setting moved or lost fails here by name.
    function test_settingIsOnItsSubPage_data() {
        return [
            { tag: "device name", entry: "settingsEntryDevice", item: "deviceNameField" },
            { tag: "language", entry: "settingsEntryDevice", item: "languageRadio" },
            { tag: "logout", entry: "settingsEntryConnection", item: "logoutButton" },
            { tag: "reset all data", entry: "settingsEntryConnection", item: "resetButton" },
            { tag: "trusted certificates", entry: "settingsEntryConnection",
              item: "trustedCertificatesHeader" },
            { tag: "forget a certificate", entry: "settingsEntryConnection", item: "forgetButton" },
            { tag: "add a favorite", entry: "settingsEntryFavorites", item: "favoriteAddButton" },
            { tag: "edit favorites", entry: "settingsEntryFavorites", item: "favoriteEditButton" },
        ]
    }

    function test_settingIsOnItsSubPage(data) {
        calaosApp.tlsPinList = [{ host: "box:443", fingerprint: "ab:cd" }]
        var entryPage = openSettings()
        compare(findChild(entryPage, data.item), null)
        clickOn(entryPage, data.entry)
        settle()
        var item = findChild(stack.currentItem, data.item)
        verify(item)
        verify(item.visible)
    }

    function test_logout() {
        clickOn(openSubPage("settingsEntryConnection"), "logoutButton")
        compare(calaosApp.logouts, 1)
    }

    function test_resetAfterTypingADeviceNameLeavesNoName() {
        var entryPage = openSettings()
        clickOn(entryPage, "settingsEntryDevice")
        settle()
        var field = findChild(stack.currentItem, "deviceNameField")
        field.forceActiveFocus()
        tryVerify(function() { return field.activeFocus })
        field.selectAll()
        keyClick("P")
        back()
        compare(hardwareUtils.saved, ["P"])

        clickOn(entryPage, "settingsEntryConnection")
        settle()
        clickOn(stack.currentItem, "resetButton")
        compare(calaosApp.resets, 1)
        back()
        compare(hardwareUtils.deviceName, "iPhone")
        compare(hardwareUtils.saved.length, 1)
    }

    function test_noCertificateHidesTheSection() {
        var page = openSubPage("settingsEntryConnection")
        var header = findChild(page, "trustedCertificatesHeader")
        verify(header)
        verify(!header.visible)
        compare(findChild(page, "forgetButton"), null)
    }

    function test_forgetACertificate() {
        calaosApp.tlsPinList = [{ host: "box:443", fingerprint: "ab:cd" }]
        clickOn(openSubPage("settingsEntryConnection"), "forgetButton")
        compare(calaosApp.forgotten, ["box:443"])
    }

    function test_languageChoice() {
        var page = openSubPage("settingsEntryDevice")
        //The active language is applied again when the radios are created.
        compare(calaosApp.languages, ["fr"])
        calaosApp.languages = []
        //The first radio is the inactive one: findChild returns it.
        clickOn(page, "languageRadio")
        compare(calaosApp.languages, ["en"])
        verify(langModel.get(0).langActive)
    }

    function test_favoritesLeadToTheirPages() {
        var page = openSubPage("settingsEntryFavorites")
        clickOn(page, "favoriteAddButton")
        compare(addSpy.count, 1)
        clickOn(page, "favoriteEditButton")
        compare(editSpy.count, 1)
    }

    function test_pagesAreInTheAppResources_data() {
        return [
            { tag: "entry", file: "SettingsView.qml" },
            { tag: "page", file: "SettingsPage.qml" },
            { tag: "row", file: "SettingsEntry.qml" },
            { tag: "device", file: "SettingsDeviceView.qml" },
            { tag: "connection", file: "SettingsConnectionView.qml" },
            { tag: "favorites", file: "SettingsFavoritesView.qml" },
        ]
    }

    function test_pagesAreInTheAppResources(data) {
        verify(harness.isResource("qml/mobile/" + data.file))
    }

    function test_captures() {
        if (shotsDir === "")
            skip("SETTINGS_SHOTS_DIR not set")
        calaosApp.tlsPinList = [{ host: "calaos.local:443",
                                  fingerprint: "4f:9a:1c:7e:22:b0:63:d8:15:ae:90:c4" }]
        var entryPage = openSettings()
        shot("settings-entry.png")
        var pages = [["settingsEntryDevice", "settings-device.png"],
                     ["settingsEntryConnection", "settings-connection.png"],
                     ["settingsEntryFavorites", "settings-favorites.png"]]
        for (var i = 0; i < pages.length; i++) {
            clickOn(entryPage, pages[i][0])
            settle()
            shot(pages[i][1])
            back()
        }
        stack.push(eventLogComponent, {}, StackView.Immediate)
        shot("settings-eventlog.png")
    }
}
