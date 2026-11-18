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

    function test_fromRangesTakesASequenceWrapper() {
        var wrapper = { length: 1, 0: range(2, 6, 0, 8, 0) }
        compare(TR.fromRanges(wrapper).length, 1)
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

    function test_slotTakesWhatItCoversFromOtherLevels() {
        var levels = [level(1, 21, [range(1, 6, 0, 9, 0)]), level(2, 18, [])]
        var r = TR.applySlot(levels, { days: [1], start: 420, end: 480, setpoint: 18 })
        verify(r.ok)
        compare(r.changed, [1, 2])
        var segs = TR.daySegments(r.levels, 1)
        compare(segs.length, 3)
        compare(segs[1].level, 2)
        compare(segs[1].start, 420)
        compare(segs[2].level, 1)
        compare(segs[2].start, 480)
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

    function test_swapLevels() {
        var levels = [level(1, 21, [range(1, 7, 0, 8, 0)]), level(2, 18, [range(2, 6, 0, 10, 0)])]
        var r = TR.swapLevels(levels, 1, 2)
        verify(r.ok)
        compare(r.levels[0].setpoint, 18)
        compare(r.levels[1].setpoint, 21)
        compare(r.levels[0].ioId, "tr1")
        compare(r.changed, [1, 2])
        compare(r.setpoints.length, 2)
        verify(!TR.swapLevels(levels, 1, 1).ok)
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
        compare(TR.replacedDays(levels, 2, [1, 3]).length, 1)
        compare(TR.replacedDays(levels, 2, [1, 3])[0].day, 1)
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
