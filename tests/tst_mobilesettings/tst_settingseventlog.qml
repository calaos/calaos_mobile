import QtQuick
import QtTest
import Calaos
import QuickFlux
import SharedComponents
import "../../qml/mobile"

//The event log page: what it shows for each state of the model, the counter
//of grouped events, and the paging it has to keep asking for.
TestCase {
    id: tc
    name: "SettingsEventLog"
    when: windowShown
    visible: true
    width: 360
    height: 640

    property bool isLandscape: false
    readonly property string shotsDir: harness.env("SETTINGS_SHOTS_DIR")

    QtObject {
        id: calaosApp
        function getPictureSized(pic) { return "qrc:/img/" + pic + ".png" }
    }

    //Same surface as EventLogModel: loadingMore shares its notify signal
    //there, it is always set before loading here.
    ListModel {
        id: eventLogModel
        property bool loading: false
        property bool loadingMore: false
        property bool error: false
        property int loadMoreCalls: 0
        property int refreshCalls: 0
        function loadMore() { loadMoreCalls++ }
        function refresh() { refreshCalls++ }
    }

    Component {
        id: viewComponent
        EventLogView { anchors.fill: parent }
    }

    function event(title, count, extra) {
        var ev = {
            evDate: "1 November", evTime: "18:42:07", evTitle: title, evCount: count,
            evType: Common.EventIoChanged, evRoomName: "Kitchen", evActionText: "On",
            evNotifText: "", evHasPicture: false, evPictureUrl: "", evIconSource: ""
        }
        for (var key in extra)
            ev[key] = extra[key]
        return ev
    }

    function fill(count) {
        for (var i = 0; i < count; i++)
            eventLogModel.append(event("Event " + i, 1))
    }

    //The list is at its end until its first layout, whatever the model holds:
    //opening a filled log asks for a page, the tests count from after that.
    function openView() {
        var view = createTemporaryObject(viewComponent, tc)
        verify(view)
        var list = findChild(view, "eventLogList")
        tryVerify(function() { return list.count === 0 || list.itemAtIndex(0) !== null })
        waitForRendering(view)
        eventLogModel.loadMoreCalls = 0
        return view
    }

    function init() {
        eventLogModel.clear()
        eventLogModel.loading = false
        eventLogModel.loadingMore = false
        eventLogModel.error = false
        eventLogModel.loadMoreCalls = 0
        eventLogModel.refreshCalls = 0
    }

    function shot(view, file) {
        if (shotsDir === "")
            return
        wait(50)
        grabImage(view).save(shotsDir + "/" + file)
    }

    function message(view) {
        return findChild(view, "eventLogMessage").text
    }

    function test_noEventSaysSo() {
        var view = openView()
        verify(findChild(view, "eventLogState").visible)
        verify(message(view).indexOf("No events in log") === 0)
        verify(!findChild(view, "eventLogRetry").visible)
    }

    function test_firstLoadIsNotCalledEmpty() {
        eventLogModel.loading = true
        var view = openView()
        verify(findChild(view, "eventLogState").visible)
        compare(message(view), "Loading…")
        verify(!findChild(view, "eventLogRetry").visible)
        //Not a page of the list yet: nothing above it to call "earlier"
        verify(!findChild(view, "eventLogMore").visible)
    }

    function test_failedLoadOffersARetry() {
        eventLogModel.error = true
        var view = openView()
        compare(message(view), "Could not load the events.")
        var retry = findChild(view, "eventLogRetry")
        verify(retry.visible)
        verify(retry.height >= Units.dp(44))
        mouseClick(retry)
        compare(eventLogModel.refreshCalls, 1)
    }

    function test_retryShowsTheLoadRatherThanTheError() {
        eventLogModel.error = true
        var view = openView()
        eventLogModel.loading = true
        compare(message(view), "Loading…")
        verify(!findChild(view, "eventLogRetry").visible)
    }

    function test_failedPageKeepsTheList() {
        fill(3)
        eventLogModel.error = true
        var view = openView()
        verify(!findChild(view, "eventLogState").visible)
        compare(findChild(view, "eventLogList").count, 3)
    }

    function test_nextPageIsShownUnderTheList() {
        fill(40)
        var view = openView()
        var more = findChild(view, "eventLogMore")
        verify(!more.visible)
        eventLogModel.loadingMore = true
        eventLogModel.loading = true
        verify(more.visible)
        verify(!findChild(view, "eventLogState").visible)
    }

    function test_reloadIsNotAnnouncedAsEarlierEvents() {
        fill(40)
        var view = openView()
        eventLogModel.loading = true
        verify(!findChild(view, "eventLogMore").visible)
    }

    function test_reachingTheEndAsksForTheNextPage() {
        fill(40)
        var view = openView()
        var list = findChild(view, "eventLogList")
        compare(eventLogModel.loadMoreCalls, 0)
        list.positionViewAtEnd()
        tryVerify(function() { return eventLogModel.loadMoreCalls === 1 })
    }

    //Without this the history stops there: the list does not grow, atYEnd
    //never changes.
    function test_pageMergedIntoTheLastRowAsksForTheNextOne() {
        fill(40)
        var view = openView()
        var list = findChild(view, "eventLogList")
        list.positionViewAtEnd()
        tryVerify(function() { return eventLogModel.loadMoreCalls === 1 })
        eventLogModel.loadingMore = true
        eventLogModel.loading = true
        eventLogModel.setProperty(39, "evCount", 21)
        eventLogModel.loadingMore = false
        eventLogModel.loading = false
        compare(eventLogModel.loadMoreCalls, 2)
    }

    function test_failedPageIsNotAskedForInALoop() {
        fill(40)
        var view = openView()
        findChild(view, "eventLogList").positionViewAtEnd()
        tryVerify(function() { return eventLogModel.loadMoreCalls === 1 })
        eventLogModel.loading = true
        eventLogModel.error = true
        eventLogModel.loading = false
        compare(eventLogModel.loadMoreCalls, 1)
    }

    function test_pageAwayFromTheEndAsksForNothing() {
        fill(40)
        openView()
        eventLogModel.loading = true
        eventLogModel.loading = false
        compare(eventLogModel.loadMoreCalls, 0)
    }

    function test_counterShownFromTwoOccurrences_data() {
        return [
            { tag: "single", count: 1, shown: false },
            { tag: "two", count: 2, shown: true },
            { tag: "many", count: 128, shown: true },
        ]
    }

    function test_counterShownFromTwoOccurrences(data) {
        eventLogModel.append(event("Light kitchen", data.count))
        var view = openView()
        var counter = findChild(findChild(view, "eventLogList"), "eventCount")
        verify(counter)
        compare(counter.visible, data.shown)
    }

    function test_rowIconKeepsItsSizeAndRatio_data() {
        return [
            { tag: "square", icon: "icon_temp", narrower: false },
            { tag: "notif 22x25", icon: "icon_notif", narrower: true },
        ]
    }

    function test_rowIconKeepsItsSizeAndRatio(data) {
        eventLogModel.append(event("Icon", 1, { evIconSource: data.icon }))
        var view = openView()
        var icon = findChild(findChild(view, "eventLogList"), "eventLogIcon")
        verify(icon)
        tryCompare(icon, "status", Image.Ready)
        compare(icon.width, Units.dp(22))
        compare(icon.height, Units.dp(22))
        compare(icon.paintedHeight, Units.dp(22))
        compare(icon.paintedWidth < icon.paintedHeight, data.narrower)
    }

    function test_counterHoldsItsTextUnderTheTime() {
        eventLogModel.append(event("Light kitchen", 128))
        var view = openView()
        var counter = findChild(findChild(view, "eventLogList"), "eventCount")
        compare(counter.height, Units.dp(24))
        verify(counter.width >= Units.dp(32))
        compare(counter.Accessible.name, "128 times")
    }

    function test_longTitleWrapsInsteadOfBeingCut() {
        eventLogModel.append(event("A title long enough to need several lines on a phone, " +
                                   "with the time and its counter on the left of it", 3))
        eventLogModel.append(event("Short", 1))
        var view = openView()
        var list = findChild(view, "eventLogList")
        var first = list.itemAtIndex(0)
        var second = list.itemAtIndex(1)
        verify(first.height > second.height)
        verify(first.width === list.width)
    }

    function test_captures() {
        if (shotsDir === "")
            skip("SETTINGS_SHOTS_DIR not set")

        eventLogModel.append(event("Light kitchen", 5))
        eventLogModel.append(event("Shutter living room", 1,
                                   { evRoomName: "Living room", evActionText: "Closed" }))
        eventLogModel.append(event("A title long enough to need several lines on a phone", 12))
        eventLogModel.append(event("Front door", 1, { evType: Common.EventPush,
                                                      evNotifText: "Someone rang at the door" }))
        eventLogModel.append(event("Temperature", 2, { evDate: "31 October" }))
        var view = openView()
        shot(view, "settings-eventlog.png")

        eventLogModel.loadingMore = true
        eventLogModel.loading = true
        shot(view, "settings-eventlog-more.png")

        eventLogModel.loadingMore = false
        eventLogModel.clear()
        shot(view, "settings-eventlog-loading.png")

        eventLogModel.loading = false
        shot(view, "settings-eventlog-empty.png")

        eventLogModel.error = true
        shot(view, "settings-eventlog-error.png")
    }
}
