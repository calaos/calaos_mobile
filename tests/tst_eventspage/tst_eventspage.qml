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
        property int backCalls: 0
        function handleBack() { backCalls++ }
        function goToDesktop() {}
    }

    QtObject {
        id: calaosApp
        property int applicationStatus: Common.LoggedIn
        function getPictureSized(name) { return name === "" ? "" : "qrc:/img/" + name + ".png" }
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
        calaosApp.applicationStatus = Common.LoggedIn
        rootWindow.backCalls = 0
    }

    function cleanup() {
        harness.setLanguage("")
    }

    function fill() {
        eventLogModel.append(event("Lamp", 1, { evIconSource: "icon_light_on" }))
        eventLogModel.append(event("Door", 3, { evIconSource: "icon_scenario" }))
        eventLogModel.append(event("Motion", 1, { evType: Common.EventPush, evNotifText: "Motion in the hall",
                                                  evHasPicture: true, evPicUid: "pic1", evIconSource: "icon_notif" }))
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

    function test_nextPageWhenTheListStillFits() {
        fill()
        var page = open()
        var list = findChild(page, "eventLogList")
        tryVerify(function() { return list.itemAtIndex(2) !== null })
        verify(list.atYEnd)
        eventLogModel.loadMoreCalls = 0

        //A failed load leaves nothing to ask for
        eventLogModel.error = true
        eventLogModel.loading = true
        eventLogModel.loading = false
        wait(50)
        compare(eventLogModel.loadMoreCalls, 0)

        eventLogModel.error = false
        eventLogModel.loading = true
        eventLogModel.loading = false
        tryVerify(function() { return eventLogModel.loadMoreCalls > 0 })
    }

    function test_reloadsAfterAReconnection() {
        fill()
        var page = open()
        compare(eventLogModel.refreshCalls, 1)

        //reset() on logout: no "No events" and no "Loading…" while the session is gone
        calaosApp.applicationStatus = Common.NotConnected
        eventLogModel.clear()
        var msg = findChild(page, "eventLogMessage")
        compare(msg.text, "Could not load the events.")
        compare(eventLogModel.refreshCalls, 1)

        calaosApp.applicationStatus = Common.Loading
        compare(msg.text, "Loading…")
        compare(eventLogModel.refreshCalls, 1)
        calaosApp.applicationStatus = Common.LoggedIn
        compare(eventLogModel.refreshCalls, 2)
        verify(msg.text.indexOf("Calaos Server") >= 0)
    }

    function test_photoClosesFromTheButtonEscapeAndBack() {
        fill()
        var page = open()
        clickRow(page, 2)
        var area = findChild(page, "eventThumbArea")
        tryVerify(function() { return area.enabled })
        var photo = findChild(page, "eventPhoto")
        var footer = findChild(page, "eventBack")

        //The footer is not under the photo
        wait(200)
        mouseClick(area)
        compare(photo.visible, true)
        verify(photo.mapToItem(page, 0, photo.height).y <= footer.mapToItem(page, 0, 0).y)
        wait(200)
        shot("events-photo")

        mouseClick(findChild(page, "eventPhotoClose"))
        compare(photo.visible, false)

        mouseClick(area)
        compare(photo.visible, true)
        keyClick(Qt.Key_Escape)
        compare(photo.visible, false)

        mouseClick(area)
        keyClick(Qt.Key_Back)
        compare(photo.visible, false)

        //Back leaves the photo first, then the page
        mouseClick(area)
        mouseClick(footer)
        compare(photo.visible, false)
        compare(rootWindow.backCalls, 0)
        mouseClick(footer)
        compare(rootWindow.backCalls, 1)
    }

    function test_retryIsTheSharedButton() {
        var page = open()
        eventLogModel.error = true
        var retry = findChild(page, "eventLogRetry")
        verify(retry.height >= Units.dp(48))
        verify(retry.hasOwnProperty("buttonClicked"))
    }

    function test_photoVeilIsNearlyOpaque() {
        fill()
        var page = open()
        verify(findChild(page, "eventPhoto").color.a >= 0.95)
    }

    function test_failureAdvisesToCheckTheConnection() {
        var page = open()
        var hint = findChild(page, "eventLogHint")
        compare(hint.visible, false)
        eventLogModel.error = true
        compare(hint.visible, true)
        compare(hint.text, "Check the connection to the box.")
    }

    function test_sameTitleOnTwoDaysIsNotSelectedTwice() {
        eventLogModel.append(event("Lamp", 1, { evDate: "1 November" }))
        eventLogModel.append(event("Lamp", 1, { evDate: "31 October" }))
        var page = open()
        var list = findChild(page, "eventLogList")
        clickRow(page, 0)
        tryVerify(function() { return list.itemAtIndex(1) !== null })
        compare(list.itemAtIndex(0).current, true)
        compare(list.itemAtIndex(1).current, false)
    }

    function test_thumbnailLoadingAndFailure() {
        fill()
        var page = open()
        clickRow(page, 2)
        var pic = eventLogModel.pictures[0]
        var loading = findChild(page, "eventThumbLoading")
        var failed = findChild(page, "eventThumbFailed")

        pic.loading = true
        compare(loading.visible, true)
        wait(100)
        shot("events-thumb-loading")

        //A failed picture has no image: nothing above the Retry button
        pic.loading = false
        pic.url = ""
        pic.failed = true
        compare(loading.visible, false)
        compare(failed.visible, true)
        var retry = findChild(page, "eventPictureRetry")
        verify(retry.height >= Units.dp(48))
        waitForRendering(page)
        wait(100)
        shot("events-thumb-failed")

        mouseClick(retry)
        compare(pic.failed, false)
        compare(failed.visible, false)
    }

    function test_frenchCapture() {
        verify(harness.setLanguage("fr"))
        fill()
        var page = open()
        wait(100)
        shot("events-list-fr")
        compare(page.title, "Historique des événements")
    }

    function test_rowShowsTheTypeIconBlueTitleAndYellowAction() {
        fill()
        var page = open()
        var list = findChild(page, "eventLogList")
        tryVerify(function() { return list.itemAtIndex(2) !== null })
        var row = list.itemAtIndex(0)
        var icon = findChild(row, "eventIcon")
        compare(icon.visible, true)
        compare(icon.source.toString(), "qrc:/img/icon_light_on.png")
        compare(findChild(row, "eventTitle").color, Theme.blueColor)
        compare(findChild(row, "eventAction").color, Theme.yellowColor)
        compare(findChild(row, "eventAction").text, "On")

        eventLogModel.setProperty(0, "evIconSource", "")
        compare(icon.visible, false)

        //A push has no action line, its text stays white
        var push = list.itemAtIndex(2)
        compare(findChild(push, "eventAction").parent.visible, false)
        compare(findChild(push, "eventNotif").visible, true)
        compare(findChild(push, "eventNotif").color, Theme.whiteColor)
        compare(findChild(push, "eventIcon").source.toString(), "qrc:/img/icon_notif.png")
    }

    function test_detailActionIsYellow() {
        fill()
        var page = open()
        clickRow(page, 0)
        var action = findChild(page, "eventDetailAction")
        compare(action.visible, true)
        compare(action.children[2].color, Theme.yellowColor)
        compare(action.children[2].text, "On")
        compare(action.children[0].color, Theme.whiteColor)
        wait(100)
        shot("events-detail-action")
    }

    readonly property string longRoom: "Salle de reunion du deuxieme etage du batiment principal, cote jardin et terrasse couverte, " +
                                       "pres de l'escalier de service, derriere la cuisine collective du personnel et des visiteurs"

    function test_longRoomElidesAndKeepsTheActionReadable() {
        eventLogModel.append(event("Light", 1, { evRoomName: longRoom, evActionText: "On" }))
        var page = open()
        var list = findChild(page, "eventLogList")
        tryVerify(function() { return list.itemAtIndex(0) !== null })
        var row = list.itemAtIndex(0)
        var action = findChild(row, "eventAction")
        compare(action.width, action.implicitWidth)
        verify(action.width > 0)
        var room = action.parent.children[0]
        verify(room.truncated)
        verify(room.width > 0)
        verify(room.width + action.parent.children[1].width + action.width <= action.parent.width + 1)
        wait(100)
        shot("events-list-long-room")
    }

    function test_longRoomWrapsInTheDetail() {
        eventLogModel.append(event("Light", 1, { evRoomName: longRoom, evActionText: "On" }))
        var page = open()
        clickRow(page, 0)
        var flow = findChild(page, "eventDetailAction")
        verify(flow.width > 0)
        var room = flow.children[0]
        verify(room.width <= flow.width)
        verify(room.lineCount > 1)
        verify(flow.childrenRect.width <= flow.width)
        compare(flow.children[2].text, "On")
        wait(100)
        shot("events-detail-long-room")
    }

    function test_loadingOnlyWhenSomethingLoads() {
        var page = open()
        var msg = findChild(page, "eventLogMessage")

        //Revoked credentials: reset() leaves NotConnected, nothing loads
        calaosApp.applicationStatus = Common.NotConnected
        compare(msg.text, "Could not load the events.")
        compare(findChild(page, "eventLogHint").visible, true)
        compare(findChild(page, "eventLogRetry").visible, true)

        eventLogModel.loading = true
        compare(msg.text, "Loading…")
        compare(findChild(page, "eventLogHint").visible, false)
        eventLogModel.error = true
        eventLogModel.loading = false
        compare(msg.text, "Could not load the events.")
    }

    function test_backspaceClosesThePhotoFirst() {
        fill()
        var page = open()
        clickRow(page, 2)
        var area = findChild(page, "eventThumbArea")
        tryVerify(function() { return area.enabled })
        wait(200)
        mouseClick(area)
        var photo = findChild(page, "eventPhoto")
        compare(photo.visible, true)
        keyClick(Qt.Key_Backspace)
        compare(photo.visible, false)
    }

    function test_resetClearsTheSelectionAndThePhoto() {
        fill()
        var page = open()
        clickRow(page, 2)
        var area = findChild(page, "eventThumbArea")
        tryVerify(function() { return area.enabled })
        wait(200)
        mouseClick(area)
        compare(findChild(page, "eventPhoto").visible, true)

        calaosApp.applicationStatus = Common.NotConnected
        eventLogModel.clear()
        compare(page.selected, null)
        compare(page.photoOpen, false)
        compare(findChild(page, "eventPhoto").visible, false)
        compare(findChild(page, "eventThumb").visible, false)
        compare(eventLogModel.pictures[0].released, 1)
    }

    //main.qml cannot be loaded in a test: its routing is read from the source
    function test_mainRoutesTheEntryToThePage() {
        var main = harness.resourceText("qml/desktop/main.qml")
        verify(main.length > 0)
        verify(/"config\/events"\)\s*\{\s*item = configEventsView\s*\}/.test(main))
        verify(/id: configEventsView\s*ConfigEventsView\s*\{/.test(main))
    }
}
