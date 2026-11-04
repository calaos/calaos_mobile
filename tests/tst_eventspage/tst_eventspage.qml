import QtQuick
import QtTest
import Calaos
import SharedComponents
import "../../qml/desktop"

//The event log page of the kiosk: what it shows for each state of the model,
//the detail, the picture, and the way to reach it from the config panel.
TestCase {
    id: tc
    name: "ConfigEventsView"
    when: windowShown
    visible: true
    width: 1280
    height: 800

    readonly property string shotsDir: harness.env("EVENTS_SHOTS_DIR")

    Rectangle { anchors.fill: parent; color: "#272727" }

    Item {
        id: rootWindow
        anchors.fill: parent
        function handleBack() {}
        function goToDesktop() {}
    }

    //Picture of an event: the url is an image of the resources
    Component {
        id: pictureComponent
        QtObject {
            property string url: "qrc:/img/button_widget_drag.png"
            property bool loading: false
            property bool failed: false
            property int released: 0
            function release() { released++ }
            function retry() { failed = false }
        }
    }

    //Same surface as EventLogModel
    ListModel {
        id: eventLogModel
        property bool loading: false
        property bool loadingMore: false
        property bool error: false
        property int loadMoreCalls: 0
        property int refreshCalls: 0
        property var pictures: []
        function loadMore() { loadMoreCalls++ }
        function refresh() { refreshCalls++ }
        function loadPicture(uid) {
            var p = pictureComponent.createObject(tc)
            pictures.push(p)
            return p
        }
    }

    Component {
        id: pageComponent
        ConfigEventsView { anchors.fill: parent }
    }

    function event(title, count, extra) {
        var ev = {
            evDate: "1 November", evTime: "18:42:07", evTitle: title, evCount: count,
            evType: Common.EventIoChanged, evRoomName: "Kitchen", evActionText: "On",
            evNotifText: "", evHasPicture: false, evPicUid: "", evIconSource: ""
        }
        for (var key in extra)
            ev[key] = extra[key]
        return ev
    }

    function init() {
        eventLogModel.clear()
        eventLogModel.loading = false
        eventLogModel.loadingMore = false
        eventLogModel.error = false
        eventLogModel.loadMoreCalls = 0
        eventLogModel.refreshCalls = 0
        eventLogModel.pictures = []
    }

    function cleanup() {
        harness.setLanguage("")
    }

    function fill() {
        eventLogModel.append(event("Lamp", 1))
        eventLogModel.append(event("Door", 3))
        eventLogModel.append(event("Motion", 1, { evType: Common.EventPush, evNotifText: "Motion in the hall",
                                                  evHasPicture: true, evPicUid: "pic1" }))
    }

    function open() {
        var page = createTemporaryObject(pageComponent, rootWindow)
        verify(page)
        waitForRendering(page)
        return page
    }

    function shot(name) {
        if (shotsDir !== "")
            grabImage(tc).save(shotsDir + "/" + name + ".png")
    }

    function clickRow(page, idx) {
        var list = findChild(page, "eventLogList")
        tryVerify(function() { return list.itemAtIndex(idx) !== null })
        mouseClick(findChild(list.itemAtIndex(idx), "eventRow"))
    }

    function test_reachableFromThePanel() {
        verify(harness.panelIcon("config/events") !== "")
        verify(harness.isResource("qml/desktop/ConfigEventsView.qml"))
        verify(harness.isResource(harness.panelIcon("config/events").substring(5)))
    }

    function test_loadsWhenOpened() {
        compare(eventLogModel.refreshCalls, 0)
        open()
        compare(eventLogModel.refreshCalls, 1)
    }

    function test_listAndCounter() {
        fill()
        var page = open()
        var list = findChild(page, "eventLogList")
        compare(list.count, 3)
        tryVerify(function() { return list.itemAtIndex(1) !== null })
        compare(findChild(list.itemAtIndex(1), "eventCount").visible, true)
        compare(findChild(list.itemAtIndex(0), "eventCount").visible, false)
        compare(findChild(page, "eventLogState").visible, false)
        wait(100)
        shot("events-list")
    }

    function test_detailAndPicture() {
        fill()
        var page = open()
        var detail = findChild(page, "eventDetail")
        compare(findChild(page, "eventThumb").visible, false)
        clickRow(page, 0)
        compare(findChild(page, "eventThumb").visible, false)
        clickRow(page, 2)
        compare(page.selected.title, "Motion")
        compare(eventLogModel.pictures.length, 1)
        var thumb = findChild(page, "eventThumb")
        compare(thumb.visible, true)
        compare(findChild(page, "eventPhoto").visible, false)
        wait(200)
        shot("events-detail")

        var area = findChild(page, "eventThumbArea")
        tryVerify(function() { return area.enabled })
        mouseClick(area)
        compare(findChild(page, "eventPhoto").visible, true)
        wait(200)
        shot("events-photo")
        mouseClick(findChild(page, "eventPhotoClose"))
        compare(findChild(page, "eventPhoto").visible, false)

        //Another row lets go of the picture
        clickRow(page, 0)
        compare(eventLogModel.pictures[0].released, 1)
    }

    function test_pictureReleasedWhenLeaving() {
        fill()
        var page = open()
        clickRow(page, 2)
        compare(eventLogModel.pictures.length, 1)
        page.destroy()
        wait(50)
        compare(eventLogModel.pictures[0].released, 1)
    }

    function test_emptyLoadingAndError() {
        var page = open()
        var state = findChild(page, "eventLogState")
        var msg = findChild(page, "eventLogMessage")
        compare(state.visible, true)
        verify(msg.text.indexOf("Calaos Server") >= 0)

        eventLogModel.loading = true
        compare(msg.text, "Loading…")
        eventLogModel.loading = false

        eventLogModel.error = true
        compare(msg.text, "Could not load the events.")
        var retry = findChild(page, "eventLogRetry")
        compare(retry.visible, true)
        waitForRendering(page)
        eventLogModel.refreshCalls = 0
        mouseClick(retry)
        compare(eventLogModel.refreshCalls, 1)
        shot("events-error")
    }

    function test_asksForTheNextPageAtTheEnd() {
        for (var i = 0; i < 60; i++)
            eventLogModel.append(event("Event " + i, 1))
        var page = open()
        var list = findChild(page, "eventLogList")
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        eventLogModel.loadMoreCalls = 0
        list.positionViewAtEnd()
        tryVerify(function() { return eventLogModel.loadMoreCalls > 0 })
    }

    function test_frenchCapture() {
        verify(harness.setLanguage("fr"))
        fill()
        var page = open()
        wait(100)
        shot("events-list-fr")
        compare(page.title, "Historique des événements")
    }
}
