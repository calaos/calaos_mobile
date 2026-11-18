import QtQuick
import QtTest
import SharedComponents

//The program card: the switch, what is active and next, the auto range, and
//its absence on a device without a program.
TestCase {
    id: tc
    name: "ClimateProgramCard"
    when: windowShown
    visible: true
    width: 420
    height: 700

    property real fakeNow: Date.parse("2026-11-18T10:00:00+01:00")
    function fakeClock() { return fakeNow }

    QtObject {
        id: fake
        signal finished(string requestId, bool ok, var answer, string error)
        property var calls: []
        property int seq: 0
        function setState(id, value) {
            calls = calls.concat([{ id: id, value: value }])
            return "r" + (++seq)
        }
    }

    Component { id: modelComponent; ClimateModel { clock: tc.fakeClock } }
    Component {
        id: cardComponent
        ClimateProgramCard { width: 380; client: fake }
    }
    Component { id: spyComponent; SignalSpy { signalName: "editRequested" } }

    function program(over) {
        var p = { enabled: true, mode_set: "heat",
                  heat: { levels: 2, active_level: 2, setpoint: 20.5, default: 17 },
                  cool: { levels: 0, active_level: null, setpoint: null, default: null },
                  next_change: { at: "2026-11-18T13:20:00+01:00", set: "heat", level: 1, setpoint: 19 } }
        for (var k in over) {
            if (over[k] === undefined)
                delete p[k]
            else
                p[k] = over[k]
        }
        return p
    }

    function make(prog, withProgramCaps) {
        var st = { mode: prog.mode_set === "auto" ? "auto" : "heat", current_temp: 21, target_temp: 22,
                   target_temp_low: 20, target_temp_high: 24, program: prog }
        var c = { modes: ["off", "heat", "cool", "auto"],
                  target_temp: { min: 16, max: 30, step: 0.5, unit: "°C", min_range: 2 },
                  target_temp_range: true }
        if (withProgramCaps !== false)
            c.program = { heat_levels: 2, cool_levels: 2, max_levels: 6 }
        var io = harness.makeClimate({ id: "salon_ac", name: "Clim", gui_type: "climate", state: "heat", rw: "true",
                                       device: { family: "climate", impl: "proxy", capabilities: c, state: st,
                                                 ios: { mode: "m", program_enabled: "pe" } } })
        var m = createTemporaryObject(modelComponent, tc, { modelData: io })
        var card = createTemporaryObject(cardComponent, tc, { climate: m })
        verify(card !== null)
        return card
    }

    function init() {
        harness.reset()
        fake.calls = []
        fakeNow = Date.parse("2026-11-18T10:00:00+01:00")
    }

    function test_displayShowsLevelAndNextChange() {
        var card = make(program())
        verify(card.visible)
        var lvl = findChild(card, "programLevel")
        verify(lvl !== null)
        verify(lvl.text.indexOf("2") >= 0)
        verify(findChild(card, "programNext").text.indexOf("3 h 20") >= 0)
        verify(!findChild(card, "programRange").visible)
    }

    function test_noChangeWithinSevenDays() {
        var card = make(program({ next_change: null }))
        verify(findChild(card, "programNext").visible)
    }

    function test_toggleWritesProgramEnabled() {
        var card = make(program())
        mouseClick(findChild(card, "programSwitch"))
        compare(fake.calls.length, 1)
        compare(fake.calls[0].id, "pe")
        compare(fake.calls[0].value, "false")
        verify(!card.programOn)
        verify(findChild(card, "programPaused").visible)
    }

    function test_refusedWriteRestoresTheSwitch() {
        var card = make(program())
        mouseClick(findChild(card, "programSwitch"))
        verify(!card.programOn)
        fake.finished("r" + fake.seq, false, null, "denied")
        verify(card.programOn)
    }

    function test_pausedProgramHasNoNextChange() {
        var card = make(program({ enabled: false, next_change: null }))
        verify(findChild(card, "programPaused").visible)
        verify(!findChild(card, "programNext").visible)
    }

    function test_autoShowsRangeAndHelp() {
        var card = make(program({ mode_set: "auto",
            heat: { levels: 2, active_level: 1, setpoint: 21, default: 17 },
            cool: { levels: 2, active_level: null, setpoint: 25, default: 28 } }))
        verify(findChild(card, "programRange").visible)
        verify(!findChild(card, "programHelp").visible)
        mouseClick(findChild(card, "programHelpButton"))
        verify(findChild(card, "programHelp").visible)
        compare(card.activeLines.length, 2)
    }

    function test_rangeHiddenOutsideAuto() {
        var card = make(program())
        verify(!findChild(card, "programRange").visible)
    }

    function test_absentWithoutProgramCapability() {
        var card = make(program(), false)
        verify(!card.visible)
    }

    function test_editRequestedFromTheButton() {
        var card = make(program())
        var spy = createTemporaryObject(spyComponent, tc, { target: card })
        mouseClick(findChild(card, "programEdit"))
        compare(spy.count, 1)
    }
}
