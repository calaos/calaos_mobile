.pragma library

//Values the user has chosen and the server has not confirmed yet.
//`pending` is what was typed or tapped and is still waiting to be sent (a burst
//of taps makes one command); `flights` are the commands sent and not settled,
//oldest first, each with the fields it changed and the number it was sent
//under. The server answers a command at once, accepted or refused, but
//publishes the new state later (a proxy reads it back from its device): a
//flight ends when the state shows what it sent, or when its command is refused.

function empty() {
    return { pending: {}, flights: [] }
}

function hasOwn(o, k) {
    return o !== null && typeof o === "object" && Object.prototype.hasOwnProperty.call(o, k)
}

//What the device is expected to hold for `key`, latest intention first.
function shown(d, key, state) {
    if (hasOwn(d.pending, key))
        return d.pending[key]
    for (var i = d.flights.length - 1; i >= 0; i--) {
        if (hasOwn(d.flights[i].fields, key))
            return d.flights[i].fields[key]
    }
    return hasOwn(state, key) ? state[key] : undefined
}

function queue(d, fields) {
    var pending = {}, k
    for (k in d.pending)
        pending[k] = d.pending[k]
    for (k in fields)
        pending[k] = fields[k]
    return { pending: pending, flights: d.flights }
}

function hasPending(d) {
    return Object.keys(d.pending).length > 0
}

//Moves the pending fields into a new flight. A field is only followed when what
//is sent differs from what was expected: nothing will be published for a
//value the device already holds, and waiting for it would pin the display.
//`fields` is the command to send; `added` says whether a flight was created for
//it, to be numbered with tag() once the command has been sent.
function send(d, state, extra) {
    var sentFields = {}, tracked = {}, any = false, k
    var all = {}
    for (k in d.pending)
        all[k] = d.pending[k]
    for (k in extra)
        all[k] = extra[k]

    var before = { pending: {}, flights: d.flights }
    for (k in all) {
        sentFields[k] = all[k]
        if (shown(before, k, state) !== all[k]) {
            tracked[k] = all[k]
            any = true
        }
    }
    var flights = d.flights.slice()
    if (any)
        flights.push({ fields: tracked })
    return { drafts: { pending: {}, flights: flights }, fields: sentFields, added: any }
}

//Numbers the flight that send() just added.
function tag(d, seq) {
    if (d.flights.length === 0 || d.flights[d.flights.length - 1].seq !== undefined)
        return d
    var flights = d.flights.slice()
    var last = flights[flights.length - 1]
    flights[flights.length - 1] = { fields: last.fields, seq: seq }
    return { pending: d.pending, flights: flights }
}

function dropEmpty(flights) {
    return flights.filter(function(f) { return Object.keys(f.fields).length > 0 })
}

function without(flight, key) {
    var f = {}
    for (var k in flight.fields) {
        if (k !== key)
            f[k] = flight.fields[k]
    }
    return { fields: f, seq: flight.seq }
}

//A new state arrived. For every followed field whose published value changed:
//- a value some command sent ends that command and every older one on this
//  field, but not a newer one: the same value can be sent twice (21.5, 21,
//  21.5) and the answer to the first must not erase the third;
//- a value nobody sent is somebody else's change: the server wins, the field
//  is released from all commands.
//An unchanged value says nothing (an unrelated frame carries the old one).
function received(d, previous, state) {
    if (d.flights.length === 0)
        return d
    var keys = {}, i, k
    for (i = 0; i < d.flights.length; i++) {
        for (k in d.flights[i].fields)
            keys[k] = true
    }

    var flights = d.flights
    for (k in keys) {
        if (!hasOwn(state, k))
            continue
        if (hasOwn(previous, k) && previous[k] === state[k])
            continue

        var match = -1
        for (i = 0; i < flights.length; i++) {
            if (hasOwn(flights[i].fields, k) && flights[i].fields[k] === state[k]) {
                match = i
                break
            }
        }
        var upTo = match < 0 ? flights.length - 1 : match
        flights = flights.map(function(f, idx) { return idx <= upTo ? without(f, k) : f })
    }
    return { pending: d.pending, flights: dropEmpty(flights) }
}

//The server answered the command `seq`. A refusal ends that flight, whatever
//else is in flight; an acceptance changes nothing, the state is still to come.
function answered(d, seq, ok) {
    if (ok || !(seq > 0))
        return d
    return { pending: d.pending,
             flights: d.flights.filter(function(f) { return f.seq !== seq }) }
}

//Nothing was heard for too long: whatever the server shows is the truth.
function expired(d) {
    return { pending: d.pending, flights: [] }
}

function inFlight(d) {
    return d.flights.length
}
