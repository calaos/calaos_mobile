pragma ComponentBehavior: Bound

import QtQuick
import QtTest
import SharedComponents
import "../../qml/SharedComponents/ClimateLogic.js" as Logic

//Renders the list item in every state of the mockups (round5/final) and
//checks the pixels the design depends on: the colour of the dial arc and of the
//knobs. With CLIMATE_SHOTS_DIR set the images are also written there.
TestCase {
    id: tc
    name: "ClimateShots"
    when: windowShown
    visible: true
    width: 1600
    height: 1800

    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")

    function caps(over) {
        var c = {
            modes: ["off", "heat", "cool", "auto", "dry", "fan_only"],
            target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
            target_temp_range: true,
            fan_modes: ["auto", "quiet", "low", "medium", "high"],
            swing_modes: ["off", "vertical"],
            reports_action: true
        }
        for (var k in over)
            c[k] = over[k]
        return c
    }

    //Same data as climate-mockups/round5/shared/data.js
    readonly property var mockStates: [
        { id: "cool", name: "Clim salon", st: { mode: "cool", current_temp: 25.4, target_temp: 22, target_temp_low: 20, target_temp_high: 24, action: "cooling" } },
        { id: "heat", name: "PAC chambre", st: { mode: "heat", current_temp: 18.6, target_temp: 20.5, action: "heating" } },
        { id: "autoIdle", name: "Clim bureau", st: { mode: "auto", current_temp: 22.1, target_temp: 22, target_temp_low: 21, target_temp_high: 23, action: "idle" } },
        { id: "autoHeat", name: "PAC séjour", st: { mode: "auto", current_temp: 18.9, target_temp: 22, target_temp_low: 19.5, target_temp_high: 24.5, action: "heating" } },
        { id: "wait", name: "Radiateur salle de bain", st: { mode: "heat", current_temp: 21, target_temp: 22.5, action: "idle", lockout: { output: "heat", remaining: 140 } } },
        { id: "off", name: "Chauffage garage", st: { mode: "off", current_temp: 12.3, target_temp: 15, action: "idle" } },
        { id: "unknown", name: "Clim cuisine", st: { mode: "", current_temp: 23, action: "idle" } },
        { id: "offline", name: "Clim véranda", st: { mode: "heat", current_temp: 19.8, target_temp: 21, action: "heating", connected: false } },
        { id: "dry", name: "Clim cellier", st: { mode: "dry", current_temp: 20.2, target_temp: 20, action: "drying", humidity: 71 } },
        //Not in the mockups: a wide idle range, with the measure away from its middle
        { id: "autoWide", name: "Clim loft", st: { mode: "auto", current_temp: 17, target_temp_low: 18, target_temp_high: 26, action: "idle" } },
        { id: "fault", name: "Plancher salon", st: { mode: "cool", current_temp: 24.1, target_temp: 22, action: "idle", fault: "humidity_high" } }
    ]

    Component {
        id: columnComponent

        Rectangle {
            id: frame
            property int itemWidth: 390
            property var picks: []
            property var items: []
            color: "#080808"
            width: itemWidth + 20
            height: col.implicitHeight + 20

            Column {
                id: col
                x: 10; y: 10
                spacing: 10

                Repeater {
                    model: frame.picks

                    delegate: Loader {
                        id: slot
                        required property var modelData
                        width: frame.itemWidth
                        sourceComponent: IOClimate {
                            clock: function() { return 1000000 }
                            modelData: harness.makeClimate({
                                id: "io_" + slot.modelData.id, name: slot.modelData.name, gui_type: "climate",
                                state: slot.modelData.st.mode, rw: "true",
                                device: { family: "climate", impl: "proxy", capabilities: tc.caps(), state: slot.modelData.st,
                                          ios: { mode: "m", target_temp: "t", target_temp_low: "l", target_temp_high: "h" } } })
                        }
                    }
                }
            }
        }
    }

    function render(picks, width) {
        var f = createTemporaryObject(columnComponent, tc, { picks: picks, itemWidth: width })
        verify(f !== null)
        waitForRendering(f)
        return f
    }

    function pick(ids) {
        return mockStates.filter(function(s) { return ids.indexOf(s.id) >= 0 })
    }

    function itemOf(f, id) {
        for (var i = 0; i < f.picks.length; i++)
            if (f.picks[i].id === id)
                return col(f).children[i].item
        return null
    }

    function col(f) { return f.children[0] }

    function find(item, name) {
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var r = find(item.children[i], name)
            if (r)
                return r
        }
        return null
    }

    //The mockups are 2x captures: draw the frame again at twice the size rather than scaling pixels
    Component {
        id: doubled
        Item {
            property Item source
            width: source.width * 2
            height: source.height * 2
            Component.onCompleted: {
                source.parent = this
                source.transformOrigin = Item.TopLeft
                source.scale = 2
            }
        }
    }

    function saveScaled(f, file) {
        var holder = createTemporaryObject(doubled, tc, { source: f })
        waitForRendering(holder)
        grabImage(holder).save(shotsDir + "/" + file)
        f.parent = tc
        f.scale = 1
        holder.destroy()
    }

    function shot(f, file) {
        var img = grabImage(f)
        verify(img.width > 0)
        if (shotsDir !== "")
            saveScaled(f, file)
        return img
    }

    //Pixel under a point of the dial, in the coordinates of the item
    function dialPixel(img, f, item, angle, radiusOffset) {
        var dial = find(item, "dial")
        var r = dial.radius + (radiusOffset || 0)
        var a = (angle - 90) * Math.PI / 180
        var p = dial.mapToItem(f, dial.center + r * Math.cos(a), dial.center + r * Math.sin(a))
        var dpr = img.width / f.width
        return img.pixel(Math.round(p.x * dpr), Math.round(p.y * dpr))
    }

    function near(c, hex, tol) {
        var want = Qt.color(hex)
        var got = Qt.color(c)
        return Math.abs(got.r - want.r) < tol && Math.abs(got.g - want.g) < tol && Math.abs(got.b - want.b) < tol
    }

    function init() {
        harness.reset()
    }

    function test_theDialUsesTheColourOfTheAction() {
        var f = render(pick(["cool", "heat", "wait", "off", "offline", "autoIdle", "autoHeat", "dry"]), 390)
        var img = shot(f, "mobile-dial-colors.png")

        // the setpoint arc, halfway between the start of the dial and the setpoint
        function fillMid(id, st) {
            var low = -150
            var hi = Logic.dialAngle(st, { min: 16, max: 30 })
            return dialPixel(img, f, itemOf(f, id), (low + hi) / 2)
        }
        verify(near(fillMid("cool", 22), "#3ab4d7", 0.16), "cooling arc is cyan")
        verify(near(fillMid("heat", 20.5), "#ffda5a", 0.16), "heating arc is yellow")
        verify(near(fillMid("wait", 22.5), "#e0a356", 0.16), "relay wait arc is amber")
        verify(near(fillMid("dry", 20), "#3ab4d7", 0.16), "drying arc is cyan")
        verify(near(fillMid("offline", 21), "#4a4a4a", 0.16), "offline arc is dark grey")

        // beyond the setpoint the track stays neutral, never the colour of the action
        var track = dialPixel(img, f, itemOf(f, "cool"), 120)
        verify(!near(track, "#3ab4d7", 0.2), "the track is not coloured")
        verify(!near(track, "#080808", 0.02), "the track is drawn")
    }

    function test_autoDrawsTwoKnobsAndAColouredBandOnlyWhileActing() {
        var f = render(pick(["autoIdle", "autoWide", "autoHeat"]), 390)
        var img = shot(f, "mobile-auto.png")
        var angle = function(t) { return Logic.dialAngle(t, { min: 16, max: 30 }) }

        var idle = itemOf(f, "autoIdle")
        verify(near(dialPixel(img, f, idle, angle(21)), "#ffda5a", 0.2), "low knob is yellow")
        verify(near(dialPixel(img, f, idle, angle(23)), "#3ab4d7", 0.2), "high knob is cyan")
        var wide = itemOf(f, "autoWide")
        verify(near(dialPixel(img, f, wide, (angle(18) + angle(26)) / 2), "#4a4a4a", 0.16), "idle band is grey")

        var heat = itemOf(f, "autoHeat")
        verify(near(dialPixel(img, f, heat, (angle(19.5) + angle(24.5)) / 2), "#ffda5a", 0.16), "acting band is yellow")
    }

    function test_statesAreRenderedForMobileAndDesktop() {
        var mobile = render(pick(["cool", "heat", "autoIdle", "autoHeat", "wait"]), 390)
        var img = shot(mobile, "mobile-list-states.png")
        verify(img.width >= 390)

        shot(render(pick(["off", "unknown", "offline", "dry", "fault"]), 390), "mobile-list-states-2.png")
        //A column of the desktop room view is about 366 wide
        shot(render(pick(["cool", "heat", "autoIdle", "autoHeat", "wait"]), 366), "desktop-list-states.png")
        shot(render(pick(["off", "unknown", "offline", "dry", "fault"]), 366), "desktop-list-states-2.png")
    }
}
