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
        id: adminBiometrics
        property bool available: false
        property bool stored: false
        property string name: "Face ID"
        property var kept: []
        property int disabled: 0
        function unlockAndKeep(password) { kept.push(password) }
        function disable() { disabled++ }
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

    function entryRect(page, name) {
        var entry = findChild(page, name)
        verify(entry, name)
        verify(entry.visible, name)
        return entry.mapToItem(page, 0, 0, entry.width, entry.height)
    }

    //In reading order: from the top, and from the left on a row
    function test_entryListsTheFiveSubPagesInOrder() {
        var page = openSettings()
        var names = ["settingsEntryDevice", "settingsEntryConnection", "settingsEntryFavorites",
                     "settingsEntryBox", "settingsEntryEventLog"]
        var previous = null
        for (var i = 0; i < names.length; i++) {
            var rect = entryRect(page, names[i])
            if (previous)
                verify(rect.y > previous.y || (rect.y === previous.y && rect.x > previous.x), names[i])
            previous = rect
        }
    }

    function test_entryTilesShareTheirRow_data() {
        return [
            { tag: "this phone", left: "settingsEntryDevice", right: "settingsEntryConnection" },
            { tag: "box", left: "settingsEntryBox", right: "settingsEntryEventLog" },
        ]
    }

    function test_entryTilesShareTheirRow(data) {
        var page = openSettings()
        var left = entryRect(page, data.left)
        var right = entryRect(page, data.right)
        compare(left.y, right.y)
        compare(left.width, right.width)
        compare(left.height, right.height)
        compare(left.x, Units.dp(16))
        compare(right.x + right.width, page.width - Units.dp(16))
        compare(right.x - (left.x + left.width), Units.dp(12))
        verify(left.height >= Units.dp(96))
    }

    function test_favoritesTakeTheWholeRow() {
        var page = openSettings()
        var favorites = entryRect(page, "settingsEntryFavorites")
        compare(favorites.x, Units.dp(16))
        compare(favorites.width, page.width - Units.dp(32))
        compare(favorites.height, Units.dp(60))
    }

    function test_longLabelGrowsItsRowAndNotOnlyItsTile() {
        var page = openSettings()
        var tile = findChild(page, "settingsEntryEventLog")
        var before = tile.height
        tile.title = "A label long enough to need several lines in a tile of half a phone"
        tryVerify(function() { return tile.height > before })
        compare(findChild(page, "settingsEntryBox").height, tile.height)
    }

    function test_everyEntryHasItsOwnIcon() {
        var page = openSettings()
        var names = ["settingsEntryDevice", "settingsEntryConnection", "settingsEntryFavorites",
                     "settingsEntryBox", "settingsEntryEventLog"]
        var seen = {}
        for (var i = 0; i < names.length; i++) {
            var glyph = findChild(page, names[i]).glyph
            verify(glyph !== "", names[i])
            verify(!seen[glyph], names[i])
            seen[glyph] = true
        }
    }

    function test_onlyTheEntriesLeavingTheSettingsHaveTheCornerArrow() {
        var page = openSettings()
        verify(findChild(findChild(page, "settingsEntryBox"), "tileOut").visible)
        verify(findChild(findChild(page, "settingsEntryEventLog"), "tileOut").visible)
        verify(!findChild(findChild(page, "settingsEntryDevice"), "tileOut").visible)
        verify(!findChild(findChild(page, "settingsEntryFavorites"), "tileOut").visible)
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

    function test_biometricsTurnedOffFromConnection() {
        adminBiometrics.stored = false
        var page = openSubPage("settingsEntryConnection")
        verify(!findChild(page, "biometricOffButton").visible)
        adminBiometrics.stored = true
        adminBiometrics.disabled = 0
        waitForRendering(page)
        clickOn(page, "biometricOffButton")
        compare(adminBiometrics.disabled, 1)
        adminBiometrics.stored = false
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

    function test_dangerZoneIsApartFromTheSession() {
        var page = openSubPage("settingsEntryConnection")
        var logout = entryRect(page, "logoutButton")
        var reset = entryRect(page, "resetButton")
        verify(reset.y >= logout.y + logout.height + Units.dp(16))
    }

    function test_tileWithAWideLabelStaysInTheColumn() {
        var page = openSettings()
        var label = findChild(findChild(page, "settingsEntryEventLog"), "tileLabel")
        var tile = findChild(page, "settingsEntryEventLog")
        verify(label.width <= tile.width)
    }

    function test_everyTileHasASubtitle() {
        var page = openSettings()
        var names = ["settingsEntryDevice", "settingsEntryConnection", "settingsEntryFavorites",
                     "settingsEntryBox", "settingsEntryEventLog"]
        for (var i = 0; i < names.length; i++) {
            var sub = findChild(findChild(page, names[i]), "tileSubtitle")
            verify(sub, names[i])
            verify(sub.visible, names[i])
            verify(sub.text !== "", names[i])
        }
    }

    function test_longSubtitleStaysInsideItsTileWhichKeepsTheWholeClickArea_data() {
        return [
            { tag: "tile", name: "settingsEntryDevice" },
            { tag: "wide tile", name: "settingsEntryFavorites" },
        ]
    }

    function test_longSubtitleStaysInsideItsTileWhichKeepsTheWholeClickArea(data) {
        var page = openSettings()
        var tile = findChild(page, data.name)
        var sub = findChild(tile, "tileSubtitle")
        var before = tile.height
        tile.subtitle ="A subtitle long enough to need several lines in a tile of half a phone, in any language"
        tryVerify(function() { return tile.height > before })
        var rect = sub.mapToItem(tile, 0, 0, sub.width, sub.height)
        verify(rect.x >= 0 && rect.y >= 0)
        verify(rect.x + rect.width <= tile.width)
        verify(rect.y + rect.height <= tile.height)
        //Children outside the parent are ignored by Qt hit testing
        var mid = sub.mapToItem(tile, sub.width / 2, sub.height / 2)
        verify(tile.contains(Qt.point(mid.x, mid.y)))
        mouseClick(tile, mid.x, mid.y)
        settle()
        verify(stack.currentItem !== page)
    }

    function test_rowsAreTouchTargets_data() {
        return [
            { tag: "logout", entry: "settingsEntryConnection", item: "logoutButton" },
            { tag: "reset", entry: "settingsEntryConnection", item: "resetButton" },
            { tag: "forget", entry: "settingsEntryConnection", item: "forgetButton" },
            { tag: "add a favorite", entry: "settingsEntryFavorites", item: "favoriteAddButton" },
            { tag: "language", entry: "settingsEntryDevice", item: "languageRadio" },
        ]
    }

    function test_rowsAreTouchTargets(data) {
        calaosApp.tlsPinList = [{ host: "box:443", fingerprint: "ab:cd" }]
        var item = findChild(openSubPage(data.entry), data.item)
        verify(item.height >= Units.dp(44))
        verify(item.width >= Units.dp(44))
    }

    function test_rowLongTitleWrapsInsideTheRow() {
        var page = openSubPage("settingsEntryFavorites")
        var row = findChild(page, "favoriteAddButton")
        var before = row.height
        row.title = "A title long enough to need more than a single line on the narrowest phone that we have to support here"
        tryVerify(function() { return row.height > before })
    }

    function test_longCertificateHostLeavesRoomForForget() {
        calaosApp.tlsPinList = [{ host: "a-very-long-host-name-of-a-box-behind-a-reverse-proxy.example.org:443",
                                  fingerprint: "4f:9a:1c:7e:22:b0:63:d8:15:ae:90:c4:4f:9a:1c:7e:22:b0:63:d8:15:ae:90:c4" }]
        var page = openSubPage("settingsEntryConnection")
        var button = entryRect(page, "forgetButton")
        compare(button.x + button.width, page.width - Units.dp(16))
    }

    function test_pagesAreInTheAppResources_data() {
        return [
            { tag: "entry", file: "SettingsView.qml" },
            { tag: "page", file: "SettingsPage.qml" },
            { tag: "tile", file: "SettingsEntry.qml" },
            { tag: "device", file: "SettingsDeviceView.qml" },
            { tag: "connection", file: "SettingsConnectionView.qml" },
            { tag: "favorites", file: "SettingsFavoritesView.qml" },
            { tag: "row", file: "SettingsRow.qml" },
            { tag: "rows", file: "SettingsRows.qml" },
            { tag: "radio", file: "SettingsRadio.qml" },
            { tag: "button", file: "SettingsButton.qml" },
            { tag: "badge", file: "IconBadge.qml" },
            { tag: "glyph", file: "SvgGlyph.qml" },
            { tag: "spinner", file: "LoadingSpinner.qml" },
            { tag: "keyboard", file: "KeyboardScroller.qml" },
        ]
    }

    function test_pagesAreInTheAppResources(data) {
        verify(harness.isResource("qml/mobile/" + data.file))
    }

    //SvgGlyph builds the path from a name: a missing file is only an empty
    //square on the phone.
    function test_iconsAreInTheAppResources_data() {
        var names = ["phone", "link", "star", "dns", "history", "tune", "add", "edit", "logout",
                     "refresh", "lock", "lock_off", "out", "chev", "check", "delete",
                     "error_outline", "spinner"]
        return names.map(function(name) { return { tag: name, name: name } })
    }

    function test_iconsAreInTheAppResources(data) {
        verify(harness.isResource("img/settings/" + data.name + ".svg"))
    }

    function test_captureBiometricOff_data() {
        return [ { tag: "faceid", name: "Face ID" }, { tag: "fingerprint", name: "" } ]
    }

    function test_captureBiometricOff(data) {
        if (shotsDir === "")
            skip("SETTINGS_SHOTS_DIR not set")
        adminBiometrics.stored = true
        adminBiometrics.name = data.name
        var page = openSubPage("settingsEntryConnection")
        shot("connection-off-" + data.tag + ".png")
        adminBiometrics.stored = false
        adminBiometrics.name = "Face ID"
    }

    function test_captures() {
        if (shotsDir === "")
            skip("SETTINGS_SHOTS_DIR not set")
        //The narrowest phone: what fits here fits everywhere
        tc.width = 360
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

        calaosApp.tlsPinList = []
        clickOn(entryPage, "settingsEntryConnection")
        settle()
        shot("settings-connection-nocert.png")
        back()

        //The keyboard is not there offscreen: its place is forced and drawn
        tc.height = 700
        clickOn(entryPage, "settingsEntryDevice")
        settle()
        var device = stack.currentItem
        var field = findChild(device, "deviceNameField")
        verify(field)
        field.forceActiveFocus()
        var scroller = findScroller(device)
        verify(scroller)
        scroller.keyboardRect = Qt.rect(0, tc.height - 300, tc.width, 300)
        keyboardBlock.visible = true
        shot("settings-device-keyboard.png")
        keyboardBlock.visible = false
        scroller.keyboardRect = Qt.rect(0, 0, 0, 0)
        back()
        tc.height = 900
        tc.width = 420
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
}
