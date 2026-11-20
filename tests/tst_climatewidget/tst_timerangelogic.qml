import QtQuick
import QtTest
import "../../qml/SharedComponents/TimeRangeLogic.js" as TR

TestCase {
    name: "TimeRangeLogic"

    function range(day, sh, sm, eh, em, es, stype) {
        return { day: String(day), start_hour: String(sh), start_min: String(sm), start_sec: "0",
                 start_type: String(stype || 0), start_offset: "0",
                 end_hour: String(eh), end_min: String(em), end_sec: String(es || 0),
                 end_type: "0", end_offset: "0" }
    }

    function level(n, setpoint, ranges) {
        return { n: n, ioId: "tr" + n, setpointId: "sp" + n, setpoint: setpoint,
                 slots: TR.fromRanges(ranges || []) }
    }

    function sixLevels() {
        var out = []
        for (var n = 1; n <= 6; n++)
            out.push(level(n, null, []))
        return out
    }

    function test_untouchedRangesGoBackAsRead_data() {
        return [
            { tag: "plain", r: range(1, 6, 0, 8, 30) },
            { tag: "seconds kept", r: range(2, 6, 0, 8, 30, 12) },
            { tag: "end of day", r: range(3, 18, 0, 23, 59, 59) },
            { tag: "wraps past midnight", r: range(7, 23, 0, 1, 0) },
            { tag: "sunrise", r: range(4, 0, 0, 9, 0, 0, 1) }
        ]
    }

    function test_untouchedRangesGoBackAsRead(d) {
        var out = TR.toRanges(TR.fromRanges([d.r]))
        compare(out.length, 1)
        compare(JSON.stringify(out[0]), JSON.stringify(d.r))
    }

    function test_wrapIsCutInTwoDays() {
        var slots = TR.fromRanges([range(7, 23, 0, 1, 0)])
        compare(slots.length, 2)
        compare(slots[0].day, 7)
        compare(slots[0].start, 23 * 60)
        compare(slots[0].end, 1440)
        //Sunday 23:00 -> monday 01:00, never sunday morning.
        compare(slots[1].day, 1)
        compare(slots[1].start, 0)
        compare(slots[1].end, 60)
        compare(slots[1].storedDay, 7)
    }

    function test_badRangesAreSkipped() {
        compare(TR.fromRanges(null).length, 0)
        compare(TR.fromRanges([null, { day: "9" }, range(0, 1, 0, 2, 0)]).length, 0)
    }

    //A server range with start == end is active for that instant only.
    function test_singleInstantRangeIsASlot() {
        var slots = TR.fromRanges([range(1, 6, 0, 6, 0)])
        compare(slots.length, 1)
        verify(slots[0].end > slots[0].start)
        verify(TR.hasSlots({ slots: slots }))
    }

    function test_singleInstantRangeSurvivesAnEdit() {
        var levels = [level(1, 20, [range(1, 6, 0, 6, 0)])]
        var r = TR.applySlot(levels, { days: [1], start: 600, end: 660, setpoint: 20 })
        verify(r.ok)
        var out = TR.toRanges(r.levels[0].slots)
        compare(out.length, 2)
        compare(out[0].start_hour, "6")
        compare(out[1].start_hour, "10")
    }

    //Not an Array (Array.isArray() is false), no Array methods, a length that
    //is not enumerable: what a list inside a QVariantMap looks like to JS.
    function sequenceWrapper(items) {
        var w = {}
        for (var i = 0; i < items.length; i++)
            w[i] = items[i]
        Object.defineProperty(w, "length", { value: items.length, enumerable: false })
        return w
    }

    //A list of a device, as the C++ side hands it over
    function test_fromRangesTakesTheListOfADevice() {
        var io = harness.makeClimate({ id: "dev_ranges", name: "Clim", gui_type: "climate", state: "heat", rw: "true",
                                       device: { family: "climate", ranges: [range(2, 6, 0, 8, 0), range(3, 7, 0, 9, 0)] } })
        compare(TR.fromRanges(io.device.ranges).length, 2)
        harness.reset()
    }

    function test_fromRangesTakesASequenceWrapper() {
        var wrapper = sequenceWrapper([range(2, 6, 0, 8, 0), range(3, 7, 0, 9, 0)])
        compare(Array.isArray(wrapper), false)
        var slots = TR.fromRanges(wrapper)
        compare(slots.length, 2)
        compare(slots[1].day, 3)
        compare(TR.fromRanges("abc").length, 0)
        compare(TR.fromRanges({}).length, 0)
    }

    function test_newSlotEndOfDayIs235959() {
        var r = TR.applySlot([level(1, 20, [])], { days: [1], start: 1080, end: 1440, setpoint: 20 })
        verify(r.ok)
        var out = TR.toRanges(r.levels[0].slots)
        compare(out.length, 1)
        compare(out[0].end_hour, "23")
        compare(out[0].end_min, "59")
        compare(out[0].end_sec, "59")
    }

    function test_newSlotPastMidnightIsSplitNotWrapped() {
        var r = TR.applySlot([level(1, 20, [])], { days: [7], start: 23 * 60, end: 60, setpoint: 20 })
        verify(r.ok)
        var out = TR.toRanges(r.levels[0].slots)
        compare(out.length, 2)
        compare(out[0].day, "1")
        compare(out[0].start_hour, "0")
        compare(out[0].end_hour, "1")
        compare(out[1].day, "7")
        compare(out[1].start_hour, "23")
        compare(out[1].end_sec, "59")
    }

    function test_sameSetpointGoesToSameLevel() {
        var levels = [level(1, 20, [range(1, 6, 0, 8, 0)]), level(2, null, [])]
        var r = TR.applySlot(levels, { days: [2], start: 360, end: 480, setpoint: 20 })
        verify(r.ok)
        compare(r.changed, [1])
        compare(r.setpoints.length, 0)
    }

    function test_newSetpointTakesFirstEmptyLevel() {
        var levels = [level(1, 20, [range(1, 6, 0, 8, 0)]), level(2, 18, []), level(3, null, [])]
        var r = TR.applySlot(levels, { days: [1], start: 1200, end: 1320, setpoint: 21.5 })
        verify(r.ok)
        compare(r.changed, [2])
        compare(r.setpoints.length, 1)
        compare(r.setpoints[0].n, 2)
        compare(r.setpoints[0].setpoint, 21.5)
    }

    function test_seventhLevelIsRefused() {
        var levels = []
        for (var n = 1; n <= 6; n++)
            levels.push(level(n, 15 + n, [range(n, 6, 0, 7, 0)]))
        var r = TR.applySlot(levels, { days: [7], start: 600, end: 660, setpoint: 25 })
        verify(!r.ok)
        compare(r.error, "max_levels")
        compare(TR.usedLevels(levels), 6)
        //An existing setpoint still fits.
        verify(TR.applySlot(levels, { days: [7], start: 600, end: 660, setpoint: 18 }).ok)
    }

    function tenLevels(taken) {
        var out = []
        for (var n = 1; n <= 10; n++)
            out.push(level(n, taken[n - 1] ? taken[n - 1].sp : null, taken[n - 1] ? taken[n - 1].ranges : []))
        return out
    }

    //Three temperatures in use, five ranges at 19: one of them moves to 19.5.
    function raoulWeek() {
        var five = []
        for (var d = 1; d <= 5; d++)
            five.push(range(d, 6, 0, 8, 0))
        return tenLevels([{ sp: 20.5, ranges: [range(6, 6, 0, 8, 0)] },
                          { sp: 19, ranges: five },
                          { sp: 21, ranges: [range(7, 6, 0, 8, 0)] }])
    }

    function test_fourthTemperatureIsRefusedAtMaxThree() {
        var r = TR.applySlot(raoulWeek(), { days: [3], start: 360, end: 480, setpoint: 19.5, maxLevels: 3,
                                            original: { day: 3, start: 360, end: 480, level: 2 } })
        verify(!r.ok)
        compare(r.error, "max_levels")
        var p = TR.previewOverlap(raoulWeek(), { days: [3], start: 360, end: 480, setpoint: 19.5, maxLevels: 3,
                                                 original: { day: 3, start: 360, end: 480, level: 2 } })
        verify(p.tooMany)
        compare(p.used, [19, 20.5, 21])
    }

    function test_fourthTemperatureIsAcceptedAtMaxTen() {
        var r = TR.applySlot(raoulWeek(), { days: [3], start: 360, end: 480, setpoint: 19.5, maxLevels: 10,
                                            original: { day: 3, start: 360, end: 480, level: 2 } })
        verify(r.ok)
        compare(r.level, 4)
        compare(TR.daySegments(r.levels, 3)[0].setpoint, 19.5)
        compare(r.levels[1].slots.length, 4)
        compare(r.levels[1].setpoint, 19)
        for (var d = 1; d <= 5; d++)
            if (d !== 3)
                compare(TR.daySegments(r.levels, d)[0].setpoint, 19)
        compare(TR.usedLevels(r.levels), 4)
    }

    function test_aLoneRangeChangesItsLevelTemperature() {
        var levels = tenLevels([{ sp: 20, ranges: [range(1, 6, 0, 8, 0)] }])
        var r = TR.applySlot(levels, { days: [1], start: 360, end: 480, setpoint: 22, maxLevels: 1,
                                       original: { day: 1, start: 360, end: 480, level: 1 } })
        verify(r.ok)
        compare(r.level, 1)
        compare(r.setpoints[0].setpoint, 22)
    }

    function test_rangePastMidnightIsEditedAsOne() {
        var levels = tenLevels([{ sp: 20, ranges: [range(7, 23, 0, 1, 0)] }, { sp: 18, ranges: [range(2, 6, 0, 7, 0)] }])
        //Edited from its second half (monday 00:00-01:00), temperature changed.
        var r = TR.applySlot(levels, { days: [1], start: 0, end: 60, setpoint: 22, maxLevels: 10,
                                       original: { day: 1, start: 0, end: 60, level: 1 } })
        verify(r.ok)
        var out = TR.toRanges(r.levels[r.level - 1].slots)
        //Both halves stay on the level (alone on it: its temperature changes).
        compare(out.length, 2)
        compare(r.level, 1)
        compare(TR.daySegments(r.levels, 7)[0].setpoint, 22)
        compare(TR.daySegments(r.levels, 1)[0].setpoint, 22)
        compare(TR.usedLevels(r.levels), 2)
    }

    function test_mostUsedSetpoint() {
        compare(TR.mostUsedSetpoint(raoulWeek(), 10), 19)
        compare(TR.mostUsedSetpoint(raoulWeek(), 1), 20.5)
        compare(TR.mostUsedSetpoint(tenLevels([]), 10), null)
    }

    function test_slotKeepsTheOtherLevelsAsTheyAre() {
        var levels = [level(1, 21, [range(1, 6, 0, 9, 0)]), level(2, 18, [])]
        var r = TR.applySlot(levels, { days: [1], start: 420, end: 480, setpoint: 18 })
        verify(r.ok)
        compare(r.changed, [2])
        compare(r.levels[0].slots.length, 1)
        compare(r.levels[0].slots[0].start, 360)
        compare(r.levels[0].slots[0].end, 540)
    }

    function test_editMovesTheSegment() {
        var levels = [level(1, 21, [range(1, 6, 0, 9, 0)])]
        var r = TR.applySlot(levels, { days: [1], start: 600, end: 660, setpoint: 21,
                                        original: { day: 1, start: 360, end: 540 } })
        verify(r.ok)
        var segs = TR.daySegments(r.levels, 1)
        compare(segs.length, 1)
        compare(segs[0].start, 600)
    }

    function test_priorityLowestLevelWins() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0)]), level(2, 18, [range(1, 6, 0, 10, 0)])]
        var segs = TR.daySegments(levels, 1)
        compare(segs.length, 3)
        compare(segs[0].level, 2)
        compare(segs[1].level, 1)
        compare(segs[1].start, 420)
        compare(segs[1].end, 480)
        compare(segs[2].level, 2)
    }

    function test_fullDayFillsTheGapsWithTheDefault() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0)])]
        var all = TR.fullDay(levels, 1, 26, true)
        compare(all.length, 3)
        compare(all[0].level, 0)
        compare(all[0].off, true)
        compare(all[1].level, 1)
        compare(all[2].end, 1440)
        compare(TR.fullDay([], 1, 19, false).length, 1)
    }

    function test_deleteCarvesEveryLevel() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0)]), level(2, 18, [range(1, 6, 0, 10, 0)])]
        var r = TR.deleteSegment(levels, 1, 420, 480)
        compare(r.changed, [1, 2])
        compare(TR.daySegments(r.levels, 1).length, 2)
        compare(TR.toRanges(r.levels[0].slots).length, 0)
    }

    function test_cutWrapGoesPlain() {
        //Cutting the monday half of a sunday wrap: sunday keeps 23:00-24:00.
        var levels = [level(1, 21, [range(7, 23, 0, 1, 0)])]
        var r = TR.deleteSegment(levels, 1, 0, 60)
        var out = TR.toRanges(r.levels[0].slots)
        compare(out.length, 1)
        compare(out[0].day, "7")
        compare(out[0].end_sec, "59")
    }

    function test_copyDayReplacesTargetsLevelByLevel() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0), range(3, 12, 0, 13, 0)]),
                      level(2, 18, [range(3, 18, 0, 20, 0)])]
        var r = TR.copyDay(levels, 1, [2, 3])
        verify(r.ok)
        compare(r.changed, [1, 2])
        var l1 = TR.toRanges(r.levels[0].slots)
        compare(l1.length, 3)
        compare(TR.toRanges(r.levels[1].slots).length, 0)
        var d3 = TR.daySegments(r.levels, 3)
        compare(d3.length, 1)
        compare(d3[0].start, 420)
    }

    function test_copyDayKeepsAWrapStoredOnItsDay() {
        var levels = [level(1, 21, [range(1, 23, 0, 1, 0)])]
        var r = TR.copyDay(levels, 1, [5])
        verify(r.ok)
        var out = TR.toRanges(r.levels[0].slots)
        compare(out.length, 2)
        compare(out[1].day, "5")
        compare(out[1].end_hour, "1")
        //The copy is idempotent: the same copy changes nothing more.
        compare(TR.copyDay(r.levels, 1, [5]).changed.length, 0)
    }

    function test_copyDayRefusals() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0)])]
        compare(TR.copyDay(levels, 2, [3]).error, "empty_source")
        compare(TR.copyDay(levels, 1, [1]).error, "no_target")
    }

    function test_levelsFromIos() {
        var ios = { heat_level1: "tr_a", heat_level1_setpoint: "sp_a", heat_level3: "tr_c_2",
                    heat_default: "hd", cool_level1: "c1", cool_default_off: "off" }
        var heat = TR.levelsFromIos(ios, "heat")
        compare(heat.length, 2)
        compare(heat[1].n, 3)
        compare(heat[1].ioId, "tr_c_2")
        compare(TR.stateIds(ios, "heat"), ["hd", "sp_a"])
        compare(TR.stateIds(ios, "cool"), ["off"])
        var withSp = TR.withSetpoints(heat, { sp_a: "20,5" })
        compare(withSp[0].setpoint, 20.5)
        compare(withSp[1].setpoint, null)
    }

    function test_rangeSpansIsWhereBothSetsHoldASetpoint() {
        var heat = [{ start: 0, end: 600, level: 1, setpoint: 20, off: false },
                    { start: 600, end: 1440, level: 0, setpoint: null, off: false }]
        var cool = [{ start: 0, end: 480, level: 0, setpoint: 28, off: true },
                    { start: 480, end: 900, level: 1, setpoint: 25, off: false },
                    { start: 900, end: 1440, level: 0, setpoint: 28, off: false }]
        compare(TR.rangeSpans(heat, cool), [{ start: 480, end: 600 }])
        compare(TR.rangeSpans([], cool), [])
    }

    function test_timeHelpers() {
        compare(TR.hhmm(1440), "24:00")
        compare(TR.hhmm(65), "01:05")
        compare(TR.parseHhmm("7:05"), 425)
        compare(TR.parseHhmm("24:00"), 1440)
        compare(TR.parseHhmm("24:01"), -1)
        compare(TR.parseHhmm("abc"), -1)
        compare(TR.snap(50, 100), 720)
        compare(TR.clampSetpoint(40), 30)
        compare(TR.clampSetpoint(20.3), 20.5)
    }
}
