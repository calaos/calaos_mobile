import QtQuick
import QtTest
import SharedComponents

//Images of the auto-mode card with the stacked strips, at the dialog width and
//at the phone width, written to CLIMATE_SHOTS_DIR when it is set.
TestCase {
    id: tc
    name: "ClimateProgramStripsShots"
    when: windowShown
    visible: true
    width: 720
    height: 420

    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")
    property real fakeNow: Date.parse("2026-11-18T10:00:00+01:00")
    function fakeClock() { return fakeNow }

    QtObject {
        id: fake
        signal finished(string requestId, bool ok, var answer, string error)
        property var reads: []
        property int seq: 0
        function getTimeRange(ioId) {
            reads = reads.concat([{ id: ioId, rid: "r" + (++seq) }])
            return "r" + seq
        }
        function getStates(items) { return "r" + (++seq) }
        function setState(id, value) { return "r" + (++seq) }
    }

    Component { id: modelComponent; ClimateModel { clock: tc.fakeClock } }
    Component { id: cardComponent; ClimateProgramCard { client: fake; now: tc.fakeNow; fontFamily: calaosFont.fontFamily } }

    function range(day, sh, eh) {
        return { day: String(day), start_hour: String(sh), start_min: "0", start_sec: "0",
                 start_type: "0", start_offset: "0", end_hour: String(eh), end_min: "0",
                 end_sec: "0", end_type: "0", end_offset: "0" }
    }

    function card(w) {
        var prog = { enabled: true, mode_set: "auto",
                     heat: { levels: 1, active_level: 1, setpoint: 21, default: 17 },
                     cool: { levels: 1, active_level: null, setpoint: 25, default: 28 },
                     next_change: { at: "2026-11-18T13:20:00+01:00", set: "heat", level: 1, setpoint: 19 } }
        var c = { modes: ["off", "heat", "cool", "auto"],
                  target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
                  target_temp_range: true, program: { heat_levels: 1, cool_levels: 1, max_levels: 6 } }
        var st = { mode: "auto", current_temp: 19.5, target_temp: 20.5, target_temp_low: 21, target_temp_high: 25, program: prog }
        var io = harness.makeClimate({ id: "strips", name: "PAC", gui_type: "climate", state: "auto", rw: "true",
            device: { family: "climate", impl: "proxy", capabilities: c, state: st,
                      ios: { mode: "m", program_enabled: "pe", heat_level1: "h1", heat_level1_setpoint: "h1sp",
                             cool_level1: "c1", cool_level1_setpoint: "c1sp" } } })
        var m = createTemporaryObject(modelComponent, tc, { modelData: io })
        fake.reads = []
        var item = createTemporaryObject(cardComponent, tc, { climate: m, x: 12, y: 12, width: w })
        tc.verify(item !== null)
        var got = fake.reads
        tc.compare(got.length, 2)
        // 2026-11-18 is day 3 of the ISO week
        fake.finished(got[0].rid, true, { ranges: [range(3, 6, 8), range(3, 17, 22)] }, "")
        fake.finished(got[1].rid, true, { ranges: [range(3, 11, 15)] }, "")
        wait(300)
        return item
    }

    function shot(file) {
        var img = grabImage(tc)
        tc.verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
    }

    function init() { harness.reset() }

    Rectangle { anchors.fill: parent; color: "#181818"; z: -1 }

    function test_stripsDialogWidth() {
        var c = card(430)
        tc.verify(c.visible)
        shot("program-auto-strips-dialog.png")
    }

    function test_stripsPhoneWidth() {
        tc.width = 390
        var c = card(366)
        tc.verify(c.visible)
        shot("program-auto-strips-phone.png")
        tc.width = 720
    }
}
