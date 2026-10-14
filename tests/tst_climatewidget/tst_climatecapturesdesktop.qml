import QtQuick
import QtTest
import SharedComponents
import "../../qml/SharedComponents/ClimateLogic.js" as Logic
import "../../qml/desktop"

//The dialogs in every state of the mockups (round4/final/desktop), drawn
//for real: the pixels of the dial arc and knobs are checked, and with
//CLIMATE_SHOTS_DIR set the images are written there for comparison.
TestCase {
    id: tc
    name: "ClimateCapturesDesktop"
    when: windowShown
    visible: true
    width: 1280
    height: 800

    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")
    property real fakeNow: 1000000
    function fakeClock() { return fakeNow }

    //Round 5 data: the same devices as the list captures
    readonly property var mock: ({
        cool: { name: "Clim salon", st: { mode: "cool", current_temp: 25.4, target_temp: 22, target_temp_low: 20, target_temp_high: 24, action: "cooling", fan_mode: "auto", swing_mode: "off", humidity: 48 } },
        heat: { name: "PAC chambre", st: { mode: "heat", current_temp: 18.6, target_temp: 20.5, action: "heating", fan_mode: "low" } },
        auto: { name: "PAC séjour", st: { mode: "auto", current_temp: 18.9, target_temp: 22, target_temp_low: 19.5, target_temp_high: 24.5, action: "heating", fan_mode: "auto" } },
        stop: { name: "Clim bureau", st: { mode: "auto", current_temp: 22.1, target_temp: 22, target_temp_low: 21, target_temp_high: 23, action: "idle", fan_mode: "auto", swing_mode: "vertical", humidity: 55 } },
        off: { name: "Chauffage garage", st: { mode: "off", current_temp: 12.3, target_temp: 15, action: "idle" } },
        offline: { name: "Clim véranda", st: { mode: "heat", current_temp: 19.8, target_temp: 21, action: "heating", connected: false } },
        unknown: { name: "Clim cuisine", st: { mode: "", current_temp: 23, action: "idle" } },
        wait: { name: "Radiateur salle de bain", st: { mode: "heat", current_temp: 21, target_temp: 22.5, action: "idle", lockout: { output: "heat", remaining: 140 } } }
    })

    function caps(extra) {
        var c = {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "quiet", "low", "medium", "high"],
            swing_modes: ["off", "vertical", "horizontal", "both"],
            reports_action: true, humidity: true
        }
        return c
    }

    //What the sheet looks like on the page behind it
    Rectangle {
        anchors.fill: parent
        color: "#080808"
    }

    //The layer main.qml gives the dialogs
    OverlayLayer {
        id: layer
        objectName: "dialogOverlayLayer"
    }

    Component {
        id: hostComponent
        DialogClimate { clock: tc.fakeClock }
    }

    function show(key) {
        var m = mock[key]
        var io = harness.makeClimate({ id: "io_" + key, name: m.name, gui_type: "climate", state: m.st.mode, rw: "true",
            device: { family: "climate", impl: "proxy", capabilities: caps(), state: m.st,
                      ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } } })
        var host = createTemporaryObject(hostComponent, tc)
        host.openFor(io)
        tryVerify(function() { return host.detail.showing && host.detail.opacity === 1 }, 3000)
        wait(400)
        return host
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

    function shot(file) {
        var img = grabImage(tc)
        verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
        return img
    }

    function dialPixel(img, angle) {
        var dial = find(layer, "bigDial")
        var a = (angle - 90) * Math.PI / 180
        var p = dial.mapToItem(null, dial.center + dial.radius * Math.cos(a), dial.center + dial.radius * Math.sin(a))
        return img.pixel(Math.round(p.x), Math.round(p.y))
    }

    function near(c, hex, tol) {
        var want = Qt.color(hex), got = Qt.color(c)
        return Math.abs(got.r - want.r) < tol && Math.abs(got.g - want.g) < tol && Math.abs(got.b - want.b) < tol
    }

    function closeAll(host) {
        if (host.modeDialog.showing)
            mouseClick(host.modeDialog.negativeButton)
        if (host.moreDialog.showing)
            mouseClick(host.moreDialog.negativeButton)
        wait(300)
        mouseClick(host.detail.negativeButton)
        tryVerify(function() { return !host.detail.showing }, 3000)
        wait(300)
        host.destroy()
        wait(50)
    }

    function init() { harness.reset() }

    readonly property var bounds: ({ min: 16, max: 30 })
    function angle(t) { return Logic.dialAngle(t, bounds) }
    function mid(t) { return (-150 + angle(t)) / 2 }

    function test_singleSetpointStates() {
        var cases = [
            { key: "cool", file: "dialog-cool.png", target: 22, color: "#3ab4d7" },
            { key: "heat", file: "dialog-heat.png", target: 20.5, color: "#ffda5a" },
            { key: "wait", file: "dialog-thermostat.png", target: 22.5, color: "#e0a356" },
            { key: "offline", file: "dialog-offline.png", target: 21, color: "#4a4a4a" },
            { key: "off", file: "dialog-off.png", target: 18, color: "#868482" }
        ]
        for (var i = 0; i < cases.length; i++) {
            var c = cases[i]
            // the off device of the mockups has a setpoint under the minimum: use one on the dial
            if (c.key === "off")
                mock.off.st.target_temp = 18
            var host = show(c.key)
            var img = shot(c.file)
            verify(near(dialPixel(img, mid(c.target)), c.color, 0.18), c.key + ": the arc has the colour of the status")
            if (c.key !== "off" && c.key !== "offline")
                verify(!near(dialPixel(img, 120), c.color, 0.1), c.key + ": beyond the setpoint the track is neutral")
            closeAll(host)
        }
    }

    function test_autoStates() {
        var host = show("auto")
        var img = shot("dialog-auto.png")
        verify(near(dialPixel(img, (angle(19.5) + angle(24.5)) / 2), "#ffda5a", 0.18), "acting band is yellow")
        verify(near(dialPixel(img, angle(19.5)), "#ffda5a", 0.2), "low knob is yellow")
        verify(near(dialPixel(img, angle(24.5)), "#3ab4d7", 0.2), "high knob is cyan")
        closeAll(host)

        host = show("stop")
        img = shot("dialog-range-stop.png")
        verify(near(dialPixel(img, angle(21)), "#ffda5a", 0.2), "low knob")
        verify(near(dialPixel(img, angle(23)), "#3ab4d7", 0.2), "high knob")
        closeAll(host)
    }

    function test_unknownMode() {
        var host = show("unknown")
        shot("dialog-unknown.png")
        compare(find(layer, "pillWord").text, "Mode unknown")
        closeAll(host)
    }

    function test_theChoiceAndTheMoreSettingsDialogs() {
        var host = show("cool")
        mouseClick(find(layer, "modePill"))
        tryVerify(function() { return host.modeDialog.showing && host.modeDialog.opacity === 1 }, 3000)
        wait(400)
        shot("popup-mode.png")
        closeAll(host)

        host = show("cool")
        mouseClick(find(layer, "moreLink"))
        tryVerify(function() { return host.moreDialog.showing && host.moreDialog.opacity === 1 }, 3000)
        wait(400)
        shot("popup-more.png")
        closeAll(host)
    }
}
