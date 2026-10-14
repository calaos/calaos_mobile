import QtQuick
import QtTest
import "../../qml/SharedComponents/ClimateDrafts.js" as Drafts

//The rules of the in-flight commands, one scenario at a time. `state` is what the
//server publishes; every command is sent after the previous one was queued.
TestCase {
    name: "ClimateDrafts"

    property var d
    property var server
    property int seq: 0

    function begin(state) {
        d = Drafts.empty()
        server = state
        seq = 0
    }

    //queue + send of one command
    function command(fields) {
        d = Drafts.queue(d, fields)
        var r = Drafts.send(d, server)
        d = r.drafts
        if (r.added)
            d = Drafts.tag(d, ++seq)
        return r.fields
    }

    function publish(state) {
        var previous = server
        server = state
        d = Drafts.received(d, previous, state)
    }

    function shown(key) {
        return Drafts.shown(d, key, server)
    }

    function test_nothingInFlightShowsTheServer() {
        begin({ target_temp: 22 })
        compare(shown("target_temp"), 22)
        compare(shown("nothing"), undefined)
        compare(Drafts.inFlight(d), 0)
    }

    function test_pendingShowsBeforeItIsSent() {
        begin({ target_temp: 22 })
        d = Drafts.queue(d, { target_temp: 23 })
        compare(shown("target_temp"), 23)
        compare(Drafts.hasPending(d), true)
        compare(Drafts.inFlight(d), 0)
    }

    function test_aSentValueStaysUntilTheServerShowsIt() {
        begin({ target_temp: 22 })
        compare(command({ target_temp: 22.5 }), { target_temp: 22.5 })
        compare(Drafts.hasPending(d), false)
        compare(shown("target_temp"), 22.5)
        publish({ target_temp: 22, current_temp: 21 })
        // an unrelated frame carrying the old value says nothing
        compare(shown("target_temp"), 22.5)
        publish({ target_temp: 22.5, current_temp: 21 })
        compare(shown("target_temp"), 22.5)
        compare(Drafts.inFlight(d), 0)
    }

    function test_aFieldEqualToWhatWasExpectedIsNotFollowed() {
        begin({ target_temp: 22 })
        command({ target_temp: 22 })
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp"), 22)

        // already 23 in flight: sending 23 again expects nothing new either
        command({ target_temp: 23 })
        command({ target_temp: 23 })
        compare(Drafts.inFlight(d), 1)
    }

    function test_threePlusThenAnOutsideChangeWins() {
        begin({ target_temp: 22 })
        command({ target_temp: 22.5 })
        command({ target_temp: 23 })
        command({ target_temp: 23.5 })
        compare(Drafts.inFlight(d), 3)
        compare(shown("target_temp"), 23.5)

        publish({ target_temp: 22.5 })
        compare(Drafts.inFlight(d), 2)
        compare(shown("target_temp"), 23.5)

        // somebody else set 25: nobody sent that, the server wins over all three
        publish({ target_temp: 25 })
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp"), 25)
    }

    function test_autoRangeLowTwiceHighThenOutsideValues() {
        begin({ target_temp_low: 20, target_temp_high: 24 })
        command({ target_temp_low: 20.5 })
        command({ target_temp_low: 21 })
        command({ target_temp_high: 24.5 })
        compare(shown("target_temp_low"), 21)
        compare(shown("target_temp_high"), 24.5)

        publish({ target_temp_low: 18, target_temp_high: 26 })
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp_low"), 18)
        compare(shown("target_temp_high"), 26)
    }

    function test_anOutsideChangeOnOneFieldLeavesTheOtherFollowed() {
        begin({ target_temp_low: 20, target_temp_high: 24 })
        command({ target_temp_low: 20.5 })
        command({ target_temp_high: 24.5 })
        publish({ target_temp_low: 20.5, target_temp_high: 26 })
        // high was changed by somebody else, low was answered
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp_high"), 26)
    }

    function test_aValueSentTwiceKeepsTheLastDraft() {
        // + - + : 21.5, 21, 21.5
        begin({ target_temp: 21 })
        command({ target_temp: 21.5 })
        command({ target_temp: 21 })
        command({ target_temp: 21.5 })
        compare(Drafts.inFlight(d), 3)

        // the answer to the first command must not erase the third
        publish({ target_temp: 21.5 })
        compare(Drafts.inFlight(d), 2)
        compare(shown("target_temp"), 21.5)

        publish({ target_temp: 21 })
        compare(Drafts.inFlight(d), 1)
        compare(shown("target_temp"), 21.5)

        publish({ target_temp: 21.5 })
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp"), 21.5)
    }

    function test_theFirstCommandRefused() {
        begin({ target_temp: 22 })
        command({ target_temp: 22.5 })
        command({ target_temp: 23 })
        d = Drafts.answered(d, 1, false)
        compare(Drafts.inFlight(d), 1)
        compare(shown("target_temp"), 23)
    }

    function test_theOnlyCommandRefusedShowsTheServerAgain() {
        begin({ target_temp: 22 })
        command({ target_temp: 22.5 })
        d = Drafts.answered(d, 1, false)
        compare(shown("target_temp"), 22)
    }

    function test_aRefusedValueSentTwiceKeepsTheLastDraft() {
        // + - + with the first refused: the third still waits on 21.5
        begin({ target_temp: 21 })
        command({ target_temp: 21.5 })
        command({ target_temp: 21 })
        command({ target_temp: 21.5 })
        d = Drafts.answered(d, 1, false)
        compare(shown("target_temp"), 21.5)
        compare(Drafts.inFlight(d), 2)
    }

    function test_anAcceptedCommandStaysUntilTheStateShowsIt() {
        begin({ target_temp: 22 })
        command({ target_temp: 22.5 })
        command({ target_temp: 23 })
        d = Drafts.answered(d, 1, true)
        compare(Drafts.inFlight(d), 2)
        d = Drafts.answered(d, 2, false)
        compare(Drafts.inFlight(d), 1)
        compare(shown("target_temp"), 22.5)
    }

    function test_anAnswerForNoCommandChangesNothing() {
        begin({ target_temp: 22 })
        command({ target_temp: 22.5 })
        d = Drafts.answered(d, 99, false)
        compare(Drafts.inFlight(d), 1)
        d = Drafts.answered(d, 0, false)
        compare(Drafts.inFlight(d), 1)
    }

    function test_aCommandThatChangedNothingHasNoFlightToTag() {
        begin({ target_temp: 22 })
        d = Drafts.queue(d, { target_temp: 22 })
        var r = Drafts.send(d, server)
        compare(r.added, false)
        compare(Drafts.inFlight(Drafts.tag(r.drafts, 5)), 0)
    }

    function test_aFieldIsNotErasedWhileANewerCommandWaitsOnIt() {
        begin({ fan_mode: "auto", target_temp: 22 })
        command({ fan_mode: "low", target_temp: 23 })
        command({ target_temp: 24 })
        // the first answers: its fan_mode is confirmed, target 23 is superseded by a newer command
        publish({ fan_mode: "low", target_temp: 23 })
        compare(shown("fan_mode"), "low")
        compare(shown("target_temp"), 24)
        compare(Drafts.inFlight(d), 1)
    }

    function test_aFieldMissingFromTheStateIsLeftAlone() {
        begin({ target_temp: 22 })
        command({ target_temp: 23 })
        publish({ current_temp: 20 })
        compare(Drafts.inFlight(d), 1)
        compare(shown("target_temp"), 23)
    }

    function test_expiredDropsEveryFlightButNotWhatIsStillBeingTyped() {
        begin({ target_temp: 22 })
        command({ target_temp: 23 })
        d = Drafts.queue(d, { target_temp: 24 })
        d = Drafts.expired(d)
        compare(Drafts.inFlight(d), 0)
        compare(shown("target_temp"), 24)
    }

    function test_oneCommandCarriesSeveralFields() {
        begin({ target_temp_low: 20, target_temp_high: 24 })
        d = Drafts.queue(d, { target_temp_low: 21 })
        d = Drafts.queue(d, { target_temp_high: 25 })
        var r = Drafts.send(d, server)
        compare(r.fields, { target_temp_low: 21, target_temp_high: 25 })
        compare(r.drafts.flights.length, 1)
    }

    function test_pendingIsNotTouchedByTheServer() {
        begin({ target_temp: 22 })
        command({ target_temp: 23 })
        d = Drafts.queue(d, { target_temp: 24 })
        publish({ target_temp: 23 })
        compare(shown("target_temp"), 24)
    }
}
