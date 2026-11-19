import QtQuick
import QtTest
import SharedComponents
import "../../qml/desktop"

//The program card and the program dialog on the desktop; images are written
//when CLIMATE_SHOTS_DIR is set.
TestCase {
    id: tc
    name: "ClimateProgramShotsDesktop"
    when: windowShown
    visible: true
    width: 1280
    height: 800

    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")
    property real fakeNow: Date.parse("2026-11-18T10:00:00+01:00")
    onFakeNowChanged: harness.setNow(fakeNow)
    function fakeClock() { return fakeNow }

    function prog(over) {
        var p = { enabled: true, mode_set: "heat",
                  heat: { levels: 2, active_level: 2, setpoint: 20.5, default: 17 },
                  cool: { levels: 0, active_level: null, setpoint: null, default: null },
                  next_change: { at: "2026-11-18T13:20:00+01:00", set: "heat", level: 1, setpoint: 19 } }
        for (var k in over)
            p[k] = over[k]
        return p
    }

    readonly property var autoProg: ({ mode_set: "auto",
        heat: { levels: 2, active_level: 1, setpoint: 21, default: 17 },
        cool: { levels: 2, active_level: null, setpoint: 25, default: 28 } })

    function make(key, mode, p, withProgram) {
        var st = { mode: mode, current_temp: 19.5, target_temp: 20.5, target_temp_low: 21, target_temp_high: 25, action: "heating" }
        if (p)
            st.program = p
        var c = { modes: ["off", "heat", "cool", "auto"],
                  target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 }, target_temp_range: true }
        if (withProgram)
            c.program = { heat_levels: 2, cool_levels: 2, max_levels: 6 }
        return harness.makeClimate({ id: "io_" + key, name: "PAC séjour", gui_type: "climate", state: mode, rw: "true",
            device: { family: "climate", impl: "proxy", capabilities: c, state: st,
                      ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h", program_enabled: "pe" } } })
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
        tc.verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
    }

    function init() { harness.reset() }
    function cleanup() { harness.reset() }

    Rectangle { anchors.fill: parent; color: "#080808" }

    OverlayLayer { id: layer; objectName: "dialogOverlayLayer" }

    Component { id: hostComponent; DialogClimate { clock: tc.fakeClock } }

    function show(io) {
        var host = createTemporaryObject(hostComponent, tc)
        host.openFor(io)
        tryVerify(function() { return host.detail.showing && host.detail.opacity === 1 }, 3000)
        wait(400)
        return host
    }

    function done(host) {
        if (host.programDialog.showing)
            mouseClick(host.programDialog.negativeButton)
        wait(300)
        mouseClick(host.detail.negativeButton)
        tryVerify(function() { return !host.detail.showing }, 3000)
        wait(300)
        host.destroy()
    }

    function test_cardStates() {
        var host = show(make("heat", "heat", prog(), true))
        shot("program-dialog-heat.png")
        tc.verify(find(layer, "programCard").visible)
        done(host)

        host = show(make("auto", "auto", prog(autoProg), true))
        mouseClick(find(layer, "programHelpButton"))
        wait(200)
        shot("program-dialog-auto-help.png")
        done(host)

        host = show(make("off", "heat", prog({ enabled: false, next_change: null }), true))
        shot("program-dialog-paused.png")
        done(host)

        host = show(make("none", "heat", null, false))
        tc.verify(!find(layer, "programCard").visible)
        shot("program-dialog-absent.png")
        done(host)
    }

    function test_programDialog() {
        var host = show(make("heat", "heat", prog(), true))
        mouseClick(find(layer, "programEdit"))
        tryVerify(function() { return host.programDialog.showing }, 3000)
        wait(500)
        shot("program-editor-dialog.png")
        done(host)
    }
}
