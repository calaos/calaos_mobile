import QtQuick
import QtQuick.Controls
import QtTest
import SharedComponents
import "../../qml/mobile"
import "../../qml/desktop"

//The French translation (lang/calaos_fr.qm, as the app installs it): every climate
//string of the .ts is translated, and the words the user reads come out in French.
//With CLIMATE_SHOTS_DIR set the list and the sheets are also written as images.
TestCase {
    id: tc
    name: "ClimateTranslation"
    when: windowShown
    visible: true
    width: 420
    height: 900

    readonly property string point: Qt.locale().decimalPoint
    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")
    property real fakeNow: 1000000
    onFakeNowChanged: harness.setNow(fakeNow)
    function fakeClock() { return fakeNow }

    Rectangle { anchors.fill: parent; color: "#080808"; z: -1 }

    OverlayLayer { id: layer; objectName: "dialogOverlayLayer" }

    Component { id: itemComponent; IOClimate { width: 390; clock: tc.fakeClock } }
    Component { id: sheetComponent; ClimateSheet { clock: tc.fakeClock } }
    Component { id: dialogComponent; DialogClimate { clock: tc.fakeClock } }

    Component {
        id: columnComponent
        Rectangle {
            property var items: []
            color: "#080808"
            width: 410
            height: col.implicitHeight + 20
            Column {
                id: col
                x: 10; y: 10
                spacing: 10
                Repeater {
                    model: parent.parent.items
                    delegate: Loader {
                        required property var modelData
                        width: 390
                        sourceComponent: itemComponent
                        onLoaded: item.modelData = modelData
                    }
                }
            }
        }
    }

    function caps() {
        return {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "quiet", "low", "medium", "high"],
            swing_modes: ["off", "vertical", "horizontal", "both"],
            reports_action: true, humidity: true
        }
    }

    readonly property var mock: ({
        cool: { name: "Clim salon", st: { mode: "cool", current_temp: 25.4, target_temp: 22, action: "cooling", fan_mode: "auto", swing_mode: "off", humidity: 48 } },
        heat: { name: "PAC chambre", st: { mode: "heat", current_temp: 18.6, target_temp: 20.5, action: "heating" } },
        auto: { name: "Clim bureau", st: { mode: "auto", current_temp: 22.1, target_temp_low: 21, target_temp_high: 23, action: "idle" } },
        wait: { name: "Radiateur salle de bain", st: { mode: "heat", current_temp: 21, target_temp: 22.5, action: "idle", lockout: { output: "heat", remaining: 140 } } },
        off: { name: "Chauffage garage", st: { mode: "off", current_temp: 12.3, target_temp: 18, action: "idle" } },
        unknown: { name: "Clim cuisine", st: { mode: "", current_temp: 23, action: "idle" } },
        offline: { name: "Clim véranda", st: { mode: "heat", current_temp: 19.8, target_temp: 21, action: "heating", connected: false } },
        dry: { name: "Clim cellier", st: { mode: "dry", current_temp: 20.2, target_temp: 20, action: "drying" } },
        stale: { name: "Plancher salon", st: { mode: "cool", current_temp: 24.1, target_temp: 22, action: "idle", fault: "humidity_high" } },
        waiting: { name: "Thermostat salon", st: { mode: "heat", current_temp: 20, target_temp: 21, action: "idle", fault: "sensor_waiting" } },
        gone: { name: "Thermostat bureau", st: { mode: "heat", current_temp: 20, target_temp: 21, action: "idle", fault: "sensor_disconnected" } }
    })

    function io(key) {
        var m = mock[key]
        return harness.makeClimate({ id: "io_" + key, name: m.name, gui_type: "climate", state: m.st.mode, rw: "true",
            device: { family: "climate", impl: "proxy", capabilities: caps(), state: m.st,
                      ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } } })
    }

    function find(item, name) {
        if (item === null || item === undefined)
            return null
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var r = find(item.children[i], name)
            if (r)
                return r
        }
        return null
    }

    //The sheet titles are all called sheetTitle: the one that says something
    function sheetTitle() {
        var found = ""
        function walk(it) {
            if (it.objectName === "sheetTitle" && it.text !== "")
                found = it.text
            for (var i = 0; i < it.children.length; i++)
                walk(it.children[i])
        }
        walk(Overlay.overlay)
        return found
    }

    function item(key) {
        var w = createTemporaryObject(itemComponent, tc, { modelData: io(key) })
        verify(w !== null)
        return w
    }

    function status(key) { return find(item(key), "statusText").text }

    function init() {
        harness.reset()
        fakeNow = 1000000
        verify(harness.setLanguage("fr"), "lang/calaos_fr.qm loads")
    }

    function cleanup() {
        harness.setLanguage("")
    }

    function shot(target, file) {
        var img = grabImage(target)
        verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
    }

    // --- the file ------------------------------------------------------------------------

    function test_everyClimateStringIsTranslated() {
        var strings = harness.climateStrings("fr")
        verify(strings.length >= 40, "the climate strings are in the .ts (" + strings.length + ")")
        var missing = strings.filter(function(s) { return !s.finished || s.translation === "" })
                             .map(function(s) { return s.context + ": " + s.source })
        compare(missing, [])
    }

    function test_theContextsOfTheClimateViewsAreThere() {
        var contexts = {}
        harness.climateStrings("fr").forEach(function(s) { contexts[s.context] = true })
        compare(Object.keys(contexts).sort(),
                ["ClimateDetail", "ClimateModel", "ClimateMore", "ClimateReading", "ClimateSheet", "DialogClimate", "IOClimate"])
    }

    // --- the list item -------------------------------------------------------------------------

    function test_statusesInFrench_data() {
        return [
            { tag: "cooling", key: "cool", text: "Refroidit" },
            { tag: "heating", key: "heat", text: "Chauffe" },
            { tag: "idle", key: "auto", text: "En veille" },
            { tag: "drying", key: "dry", text: "Déshumidifie" },
            { tag: "stopped", key: "off", text: "Arrêté" },
            { tag: "offline", key: "offline", text: "Hors ligne" },
            { tag: "lockout", key: "wait", text: "Démarre dans 2 min" },
            { tag: "humidity", key: "stale", text: "Humidité trop haute" },
            { tag: "waiting for the sensor", key: "waiting", text: "En attente de la sonde" },
            { tag: "sensor disconnected", key: "gone", text: "Sonde déconnectée" }
        ]
    }

    function test_statusesInFrench(d) {
        compare(status(d.key), d.text)
    }

    function test_modesInFrench_data() {
        return [
            { tag: "off", key: "off", word: "Arrêt" },
            { tag: "heat", key: "heat", word: "Chauffage" },
            { tag: "cool", key: "cool", word: "Froid" },
            { tag: "auto", key: "auto", word: "Auto" },
            { tag: "dry", key: "dry", word: "Déshumidification" },
            { tag: "unknown", key: "unknown", word: "Mode inconnu" }
        ]
    }

    function test_modesInFrench(d) {
        compare(find(item(d.key), "modeText").text, d.word)
    }

    function test_theSetpointUsesTheLocaleSeparator() {
        var w = item("heat")
        compare(find(w, "setpointText").text, "20" + point + "5°")
        compare(find(w, "roomTemp").text, "18" + point + "6°")
    }

    // --- the sheet ----------------------------------------------------------------------------------

    function openSheet(key) {
        var host = createTemporaryObject(sheetComponent, tc)
        host.openFor(io(key))
        tryVerify(function() { return host.opened && find(Overlay.overlay, "bigDial") !== null }, 3000)
        wait(350)
        return host
    }

    function closeSheet(host) {
        host.close()
        tryVerify(function() { return !host.opened && !host.modeOpened && !host.moreOpened }, 3000)
        host.destroy()
        wait(300)
    }

    function test_theSheetInFrench() {
        var host = openSheet("cool")
        compare(find(Overlay.overlay, "roomText").text, "Pièce 25" + point + "4°")
        compare(find(Overlay.overlay, "statusWord").text, "Refroidit")
        compare(find(Overlay.overlay, "pillWord").text, "Froid")
        shot(tc, "fr-sheet-cool.png")
        closeSheet(host)
    }

    function test_theGapInFrench() {
        var host = openSheet("auto")
        compare(find(Overlay.overlay, "stopText").text, "Écart minimum 2°")
        shot(tc, "fr-sheet-range-stop.png")
        closeSheet(host)
    }

    function test_theLockoutInSecondsInFrench() {
        var host = openSheet("wait")
        compare(find(Overlay.overlay, "statusWord").text, "Démarre dans 2 min 20")
        shot(tc, "fr-sheet-thermostat.png")
        closeSheet(host)
    }

    function test_theChoiceOfModeInFrench() {
        var host = openSheet("cool")
        mouseClick(find(Overlay.overlay, "modePill"))
        tryVerify(function() { return host.modeOpened }, 3000)
        wait(350)
        compare(sheetTitle(), "Mode")
        var labels = ["off", "heat", "cool", "auto", "dry", "fan_only"].map(function(m) {
            return find(find(Overlay.overlay, "row_" + m), "rowLabel").text })
        compare(labels, ["Arrêt", "Chauffage", "Froid", "Auto", "Déshumidification", "Ventilation"])
        shot(tc, "fr-sheet-mode.png")
        closeSheet(host)
    }

    function test_theMoreSettingsInFrench() {
        var host = openSheet("cool")
        compare(find(Overlay.overlay, "moreLink").visible, true)
        mouseClick(find(Overlay.overlay, "moreLink"))
        tryVerify(function() { return host.moreOpened }, 3000)
        wait(350)
        var fan = ["auto", "quiet", "low", "medium", "high"].map(function(m) {
            return find(find(find(Overlay.overlay, "fanRows"), "row_" + m), "rowLabel").text })
        compare(fan, ["Auto", "Silencieux", "Faible", "Moyen", "Fort"])
        var swing = ["off", "vertical", "horizontal", "both"].map(function(m) {
            return find(find(find(Overlay.overlay, "swingRows"), "row_" + m), "rowLabel").text })
        compare(swing, ["Fixe", "Vertical", "Horizontal", "Les deux"])
        compare(find(Overlay.overlay, "humidityText").text, "Humidité 48 %")
        compare(sheetTitle(), "Plus de réglages")
        shot(tc, "fr-sheet-more.png")
        closeSheet(host)
    }

    function test_theListInFrench() {
        var items = ["cool", "heat", "auto", "wait", "off", "unknown", "offline", "dry", "stale", "waiting"].map(io)
        var f = createTemporaryObject(columnComponent, tc, { items: items })
        waitForRendering(f)
        compare(find(f, "statusText").text, "Refroidit")
        shot(f, "fr-list.png")
    }

    // --- the desktop dialog --------------------------------------------------------------------------

    function test_theDialogInFrench() {
        var dlg = createTemporaryObject(dialogComponent, tc)
        dlg.openFor(io("cool"))
        tryVerify(function() { return dlg.detail.showing && dlg.detail.opacity === 1 }, 3000)
        wait(350)
        compare(find(layer, "roomText").text, "Pièce 25" + point + "4°")
        compare(dlg.detail.negativeButtonText, "Fermer")
        mouseClick(find(layer, "moreLink"))
        tryVerify(function() { return dlg.moreDialog.showing && dlg.moreDialog.opacity === 1 }, 3000)
        compare(dlg.moreDialog.title, "Plus de réglages — Clim salon")
        dlg.destroy()
        wait(100)
    }

    // --- back to English ---------------------------------------------------------------------------

    function test_withoutALanguageTheTextsAreTheSource() {
        harness.setLanguage("")
        compare(status("cool"), "Cooling")
        compare(find(item("off"), "modeText").text, "Off")
    }
}
