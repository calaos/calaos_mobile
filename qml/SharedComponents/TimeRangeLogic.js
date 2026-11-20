.pragma library
.import "ClimateLogic.js" as Climate

//Pure helpers behind TimeRangeEditor: time ranges of a climate's levels
//(get_timerange / set_timerange) <-> slots drawn on a 24 h bar.
//
//A slot is { day: 1..7, start, end (minutes, 0..1440), raw, group, parts,
//storedDay }. A range read from the server keeps its raw object: as long as
//the user does not touch it, it is written back as it came (seconds, sunrise
//and sunset types, a range wrapping past midnight). Every function returns
//new objects; untouched slots are shared.

var DayMinutes = 1440
var MaxLevels = 6

var groupSeq = 0

function toInt(v, fallback) {
    var n = parseInt(v, 10)
    return isNaN(n) ? fallback : n
}

function nextDay(day) {
    return day % 7 + 1
}

//"17:30"; 1440 is "24:00".
function hhmm(minutes) {
    var h = Math.floor(minutes / 60)
    var m = minutes % 60
    return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m
}

//"17:30", "7:05", "24:00" -> minutes; -1 when unreadable.
function parseHhmm(text) {
    var m = /^\s*(\d{1,2})\s*[:hH.]\s*(\d{2})\s*$/.exec(String(text))
    if (!m)
        return -1
    var h = parseInt(m[1], 10), mn = parseInt(m[2], 10)
    if (mn > 59 || h > 24 || (h === 24 && mn !== 0))
        return -1
    return h * 60 + mn
}

//Ranges of one get_timerange answer -> slots.
function fromRanges(ranges) {
    var slots = []
    if (!Climate.isList(ranges))
        return slots
    for (var i = 0; i < ranges.length; i++) {
        var r = ranges[i]
        if (!r || typeof r !== "object")
            continue
        var day = toInt(r.day, 0)
        if (day < 1 || day > 7)
            continue
        var start = toInt(r.start_hour, 0) * 60 + toInt(r.start_min, 0)
        var endH = toInt(r.end_hour, 0), endM = toInt(r.end_min, 0), endS = toInt(r.end_sec, 0)
        //The server's end is inclusive: 23:59:59 is the end of the day.
        var end = (endH === 23 && endM === 59 && endS === 59) ? DayMinutes : endH * 60 + endM
        //start == end is active for one second server-side (inclusive end); the grid cannot hold less than a minute
        if (end === start && start < DayMinutes)
            end = start + 1
        var group = "g" + (++groupSeq)
        if (end < start) {
            //Starts on its day and ends the next morning (InPlageHoraire).
            slots.push({ day: day, start: start, end: DayMinutes, raw: r, group: group, parts: 2, storedDay: day })
            slots.push({ day: nextDay(day), start: 0, end: end, raw: r, group: group, parts: 2, storedDay: day })
        } else {
            slots.push({ day: day, start: start, end: end, raw: r, group: group, parts: 1, storedDay: day })
        }
    }
    return slots
}

function plain(day, start, end) {
    return { day: day, start: start, end: end, raw: null, group: "", parts: 1, storedDay: day }
}

function rangeOf(day, start, end) {
    var e = end >= DayMinutes ? { h: 23, m: 59, s: 59 } : { h: Math.floor(end / 60), m: end % 60, s: 0 }
    return {
        day: String(day),
        start_hour: String(Math.floor(start / 60)), start_min: String(start % 60), start_sec: "0",
        start_type: "0", start_offset: "0",
        end_hour: String(e.h), end_min: String(e.m), end_sec: String(e.s),
        end_type: "0", end_offset: "0"
    }
}

function intact(slots, slot) {
    if (!slot.raw)
        return false
    var n = 0
    for (var i = 0; i < slots.length; i++)
        if (slots[i].raw === slot.raw && slots[i].group === slot.group)
            n++
    return n === slot.parts
}

//Slots -> the whole ranges list of the level, as set_timerange wants it.
//Untouched ranges go back as read; the others are merged per day and never
//wrap past midnight.
function toRanges(slots) {
    var out = []
    var doneGroups = {}
    var byDay = {}
    for (var i = 0; i < slots.length; i++) {
        var s = slots[i]
        if (intact(slots, s)) {
            if (!doneGroups[s.group]) {
                doneGroups[s.group] = true
                out.push(s.raw)
            }
            continue
        }
        if (s.end <= s.start)
            continue
        if (!byDay[s.day])
            byDay[s.day] = []
        byDay[s.day].push({ start: s.start, end: s.end })
    }
    for (var d = 1; d <= 7; d++) {
        var list = byDay[d]
        if (!list)
            continue
        list.sort(function(a, b) { return a.start - b.start })
        var cur = null
        for (var k = 0; k < list.length; k++) {
            if (cur && list[k].start <= cur.end) {
                cur.end = Math.max(cur.end, list[k].end)
            } else {
                if (cur)
                    out.push(rangeOf(d, cur.start, cur.end))
                cur = { start: list[k].start, end: list[k].end }
            }
        }
        if (cur)
            out.push(rangeOf(d, cur.start, cur.end))
    }
    return out
}

function sameRanges(a, b) {
    return JSON.stringify(toRanges(a)) === JSON.stringify(toRanges(b))
}

//Removes [start, end) of `day` from the slots. A slot that is cut loses its
//raw range: what is left of it is written as plain ranges.
function carve(slots, day, start, end) {
    var out = []
    for (var i = 0; i < slots.length; i++) {
        var s = slots[i]
        if (s.day !== day || s.end <= start || s.start >= end || s.end <= s.start) {
            out.push(s)
            continue
        }
        if (s.start < start)
            out.push(plain(day, s.start, start))
        if (s.end > end)
            out.push(plain(day, end, s.end))
    }
    //The other half of a wrapping range that was cut is now alone: it is
    //written plain, which intact() decides from the count of parts.
    return out
}

function cloneLevels(levels) {
    var out = []
    for (var i = 0; i < levels.length; i++) {
        var l = levels[i]
        out.push({ n: l.n, ioId: l.ioId, setpointId: l.setpointId, setpoint: l.setpoint, slots: l.slots })
    }
    return out
}

function levelIndex(levels, n) {
    for (var i = 0; i < levels.length; i++)
        if (levels[i].n === n)
            return i
    return -1
}

function hasSlots(level) {
    for (var i = 0; i < level.slots.length; i++)
        if (level.slots[i].end > level.slots[i].start)
            return true
    return false
}

function sameSetpoint(a, b) {
    return a !== null && a !== undefined && b !== null && b !== undefined &&
           Math.abs(Number(a) - Number(b)) < 0.01
}

//Grouping by temperature: the level already at this setpoint, else the first
//level without any slot (its setpoint is rewritten), else -1: all are taken.
function pickLevel(levels, setpoint) {
    var i
    for (i = 0; i < levels.length; i++)
        if (sameSetpoint(levels[i].setpoint, setpoint) && hasSlots(levels[i]))
            return levels[i].n
    for (i = 0; i < levels.length; i++)
        if (sameSetpoint(levels[i].setpoint, setpoint))
            return levels[i].n
    for (i = 0; i < levels.length; i++)
        if (!hasSlots(levels[i]))
            return levels[i].n
    return -1
}

function usedLevels(levels) {
    var n = 0
    for (var i = 0; i < levels.length; i++)
        if (hasSlots(levels[i]))
            n++
    return n
}

//Levels whose ranges differ between two states, by level number.
function changedLevels(before, after) {
    var out = []
    for (var i = 0; i < after.length; i++) {
        var j = levelIndex(before, after[i].n)
        if (j < 0 || !sameRanges(before[j].slots, after[i].slots))
            out.push(after[i].n)
    }
    return out
}

//Who wins each minute of `day`: the lowest level number (priority).
//[{ start, end, level, setpoint }] of the covered parts, in order.
function daySegments(levels, day) {
    var points = []
    var i, j
    for (i = 0; i < levels.length; i++) {
        for (j = 0; j < levels[i].slots.length; j++) {
            var s = levels[i].slots[j]
            if (s.day === day && s.end > s.start)
                points.push(s.start, s.end)
        }
    }
    points.sort(function(a, b) { return a - b })
    var segs = []
    for (var p = 0; p + 1 < points.length; p++) {
        var a = points[p], b = points[p + 1]
        if (b <= a)
            continue
        var best = null
        for (i = 0; i < levels.length; i++) {
            if (best && levels[i].n >= best.n)
                continue
            for (j = 0; j < levels[i].slots.length; j++) {
                var t = levels[i].slots[j]
                if (t.day === day && t.start <= a && t.end >= b) {
                    best = levels[i]
                    break
                }
            }
        }
        if (!best)
            continue
        var last = segs.length ? segs[segs.length - 1] : null
        if (last && last.level === best.n && last.end === a)
            last.end = b
        else
            segs.push({ start: a, end: b, level: best.n, setpoint: best.setpoint })
    }
    return segs
}

//The whole day, the gaps included: level 0 is the default setpoint, or off.
function fullDay(levels, day, defaultSetpoint, defaultOff) {
    var segs = daySegments(levels, day)
    var out = []
    var at = 0
    for (var i = 0; i < segs.length; i++) {
        if (segs[i].start > at)
            out.push({ start: at, end: segs[i].start, level: 0, setpoint: defaultSetpoint, off: !!defaultOff })
        out.push({ start: segs[i].start, end: segs[i].end, level: segs[i].level, setpoint: segs[i].setpoint, off: false })
        at = segs[i].end
    }
    if (at < DayMinutes)
        out.push({ start: at, end: DayMinutes, level: 0, setpoint: defaultSetpoint, off: !!defaultOff })
    return out
}

//The minutes where both sets hold a setpoint: the auto range exists there.
//Takes two fullDay() results, gives [{ start, end }].
function rangeSpans(heat, cool) {
    var out = []
    for (var i = 0; i < heat.length; i++) {
        if (heat[i].off || heat[i].setpoint === null || heat[i].setpoint === undefined)
            continue
        for (var j = 0; j < cool.length; j++) {
            if (cool[j].off || cool[j].setpoint === null || cool[j].setpoint === undefined)
                continue
            var a = Math.max(heat[i].start, cool[j].start)
            var b = Math.min(heat[i].end, cool[j].end)
            if (b <= a)
                continue
            var last = out.length ? out[out.length - 1] : null
            if (last && last.end === a)
                last.end = b
            else
                out.push({ start: a, end: b })
        }
    }
    return out
}

function slotCount(levels, day) {
    return daySegments(levels, day).length
}

function result(before, after, setpoints) {
    return { ok: true, error: "", levels: after, changed: changedLevels(before, after), setpoints: setpoints || [] }
}

function failure(error) {
    return { ok: false, error: error, levels: null, changed: [], setpoints: [] }
}

//Puts a slot on the bar.
//edit: { days: [1..7], start, end, setpoint, original: { day, start, end, level } }
//original is the segment being edited, absent for a new slot. end <= start
//wraps past midnight: the slot is cut in two days. Other levels are left
//alone: where ranges overlap, the lowest level number wins (see daySegments).
function applySlot(levels, edit) {
    var start = edit.start, end = edit.end
    if (start < 0 || end < 0 || start > DayMinutes || end > DayMinutes || start === end)
        return failure("invalid")
    if (!edit.days || !edit.days.length)
        return failure("no_day")

    var next = cloneLevels(levels)
    var i
    if (edit.original) {
        var o = edit.original
        for (i = 0; i < next.length; i++)
            if (!o.level || next[i].n === o.level)
                next[i].slots = carve(next[i].slots, o.day, o.start, o.end)
    }

    var pieces = pieceList(edit)
    var n = pickLevel(next, edit.setpoint)
    if (n < 0)
        return failure("max_levels")
    var idx = levelIndex(next, n)
    var slots = next[idx].slots
    for (var p = 0; p < pieces.length; p++)
        slots = carve(slots, pieces[p].day, pieces[p].start, pieces[p].end)
    for (p = 0; p < pieces.length; p++)
        slots.push(plain(pieces[p].day, pieces[p].start, pieces[p].end))
    next[idx].slots = slots

    next[idx].setpoint = Number(edit.setpoint)
    //Swap: the other level takes over this range's priority.
    if (edit.swapWith > 0 && edit.swapWith !== n) {
        var io = levelIndex(next, edit.swapWith)
        if (io >= 0) {
            var tmp = { slots: next[io].slots, setpoint: next[io].setpoint }
            next[io].slots = next[idx].slots
            next[io].setpoint = next[idx].setpoint
            next[idx].slots = tmp.slots
            next[idx].setpoint = tmp.setpoint
            n = edit.swapWith
        }
    }
    var setpoints = []
    for (i = 0; i < next.length; i++)
        if (next[i].setpoint !== null && next[i].setpoint !== undefined &&
            !sameSetpoint(next[i].setpoint, levels[i].setpoint))
            setpoints.push({ n: next[i].n, setpoint: Number(next[i].setpoint) })
    var res = result(levels, next, setpoints)
    res.level = n
    return res
}

//The day pieces of an edit: a range past midnight is two pieces.
function pieceList(edit) {
    var pieces = []
    for (var d = 0; d < edit.days.length; d++) {
        var day = edit.days[d]
        if (edit.end > edit.start) {
            pieces.push({ day: day, start: edit.start, end: edit.end })
        } else {
            pieces.push({ day: day, start: edit.start, end: DayMinutes })
            if (edit.end > 0)
                pieces.push({ day: nextDay(day), start: 0, end: edit.end })
        }
    }
    return pieces
}

//What saving `edit` would do, for the period view:
//{ tooMany, level, overlaps: [{ range: {start, end}, setpoint, level, from, to, wins }] }
//`wins` is true when the edited range has the lower level number.
function previewOverlap(levels, edit) {
    var out = { tooMany: false, level: 0, overlaps: [] }
    var res = applySlot(levels, edit)
    if (!res.ok) {
        out.tooMany = res.error === "max_levels"
        return out
    }
    out.level = res.level
    var pieces = pieceList(edit)
    for (var i = 0; i < res.levels.length; i++) {
        var lv = res.levels[i]
        if (lv.n === res.level)
            continue
        for (var j = 0; j < lv.slots.length; j++) {
            var s = lv.slots[j]
            for (var p = 0; p < pieces.length; p++) {
                var from = Math.max(s.start, pieces[p].start), to = Math.min(s.end, pieces[p].end)
                if (s.day === pieces[p].day && s.end > s.start && from < to)
                    out.overlaps.push({ range: { start: s.start, end: s.end }, setpoint: lv.setpoint,
                                        level: lv.n, from: from, to: to, wins: res.level < lv.n })
            }
        }
    }
    return out
}

//Deletes what the bar shows on [start, end) of `day`, every level included:
//what was hidden under a higher priority slot would show up otherwise.
function deleteSegment(levels, day, start, end) {
    var next = cloneLevels(levels)
    for (var i = 0; i < next.length; i++)
        next[i].slots = carve(next[i].slots, day, start, end)
    return result(levels, next, [])
}

function shiftRaw(raw, day) {
    var r = {}
    for (var k in raw)
        r[k] = raw[k]
    r.day = String(day)
    return r
}

//"Copy this day": the ranges stored on each target day are replaced by the
//ones stored on `source`, level by level. A range wrapping past midnight is
//stored on its first day and is copied with it.
function copyDay(levels, source, targets) {
    var t, i, j
    var hasSource = false
    for (i = 0; i < levels.length && !hasSource; i++)
        for (j = 0; j < levels[i].slots.length; j++)
            if (levels[i].slots[j].storedDay === source && levels[i].slots[j].end > levels[i].slots[j].start) {
                hasSource = true
                break
            }
    if (!hasSource)
        return failure("empty_source")
    var days = []
    for (t = 0; t < targets.length; t++)
        if (targets[t] !== source && days.indexOf(targets[t]) < 0)
            days.push(targets[t])
    if (!days.length)
        return failure("no_target")

    var next = cloneLevels(levels)
    for (i = 0; i < next.length; i++) {
        var kept = []
        var src = []
        var slots = levels[i].slots
        for (j = 0; j < slots.length; j++) {
            if (days.indexOf(slots[j].storedDay) < 0)
                kept.push(slots[j])
            if (slots[j].storedDay === source)
                src.push(slots[j])
        }
        for (t = 0; t < days.length; t++) {
            var shift = days[t] - source
            var groups = {}
            for (j = 0; j < src.length; j++) {
                var s = src[j]
                var day = ((s.day - 1 + shift) % 7 + 7) % 7 + 1
                if (intact(slots, s)) {
                    if (!groups[s.group])
                        groups[s.group] = { raw: shiftRaw(s.raw, days[t]), id: "g" + (++groupSeq) }
                    kept.push({ day: day, start: s.start, end: s.end, raw: groups[s.group].raw,
                                group: groups[s.group].id, parts: s.parts, storedDay: days[t] })
                } else {
                    var c = plain(day, s.start, s.end)
                    c.storedDay = days[t]
                    kept.push(c)
                }
            }
        }
        next[i].slots = kept
    }
    return result(levels, next, [])
}

//Levels of one direction ("heat" / "cool") from device.ios (role -> id):
//ids are read, never rebuilt (they may carry a _N suffix).
function levelsFromIos(ios, kind) {
    var out = []
    if (!ios || typeof ios !== "object")
        return out
    for (var n = 1; n <= MaxLevels; n++) {
        var id = ios[kind + "_level" + n]
        if (typeof id !== "string" || id === "")
            continue
        var sp = ios[kind + "_level" + n + "_setpoint"]
        out.push({ n: n, ioId: id, setpointId: typeof sp === "string" ? sp : "", setpoint: null, slots: [] })
    }
    return out
}

//Same ids, same key: JSON.stringify depends on the key order
function iosKey(ios) {
    if (!ios || typeof ios !== "object")
        return ""
    return Object.keys(ios).sort().map(function(k) { return k + "=" + ios[k] }).join("|")
}

//The state ids to read for one direction: level setpoints and defaults.
function stateIds(ios, kind) {
    var out = []
    if (!ios || typeof ios !== "object")
        return out
    var roles = [kind + "_default"]
    if (kind === "cool")
        roles.push("cool_default_off")
    for (var n = 1; n <= MaxLevels; n++)
        roles.push(kind + "_level" + n + "_setpoint")
    for (var i = 0; i < roles.length; i++)
        if (typeof ios[roles[i]] === "string" && ios[roles[i]] !== "")
            out.push(ios[roles[i]])
    return out
}

function toSetpoint(v) {
    if (v === undefined || v === null || v === "")
        return null
    var n = Number(String(v).replace(",", "."))
    return isNaN(n) ? null : n
}

//Fills the setpoints of the levels from a get_state answer ({ id: value }).
function withSetpoints(levels, states) {
    var next = cloneLevels(levels)
    for (var i = 0; i < next.length; i++)
        if (next[i].setpointId && states && states.hasOwnProperty(next[i].setpointId))
            next[i].setpoint = toSetpoint(states[next[i].setpointId])
    return next
}

function withSlots(levels, n, slots) {
    var next = cloneLevels(levels)
    var i = levelIndex(next, n)
    if (i >= 0)
        next[i].slots = slots
    return next
}

function levelById(levels, ioId) {
    for (var i = 0; i < levels.length; i++)
        if (levels[i].ioId === ioId)
            return levels[i]
    return null
}

//Setpoint bounds and steps of the form.
var SetpointMin = 7
var SetpointMax = 30
var SetpointStep = 0.5
var TimeStep = 15

function clampSetpoint(v) {
    var n = Math.round(Number(v) / SetpointStep) * SetpointStep
    return Math.min(SetpointMax, Math.max(SetpointMin, n))
}

//Minutes of a drag on a bar of `width` pixels, on the 15 min grid.
function snap(x, width) {
    if (width <= 0)
        return 0
    var m = Math.round(x / width * DayMinutes / TimeStep) * TimeStep
    return Math.min(DayMinutes, Math.max(0, m))
}

//Today as 1 (monday) .. 7 (sunday).
function isoDay(date) {
    var d = date.getDay()
    return d === 0 ? 7 : d
}

//Colour of a level on the bar and in the period rows: shades of the theme
//colour `base` (lighter = higher priority); `none` outside 1..6.
function levelColor(base, n, none) {
    if (n < 1 || n > MaxLevels)
        return none
    var f = [1.35, 1.15, 1.0, 0.85, 0.7, 0.55][n - 1]
    return f >= 1 ? Qt.lighter(base, f) : Qt.darker(base, 1 / f)
}
