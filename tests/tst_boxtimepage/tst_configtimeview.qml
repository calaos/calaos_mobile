import QtQuick
import QtTest
import Calaos
import SharedComponents
import QuickFlux
import "../../qml/desktop"

//The page on the real manager and the real API: what it shows comes from the
//fake daemon, and what it asks of the box is read back from the daemon.
TestCase {
    id: tc
    name: "ConfigTimeView"
    when: windowShown
    visible: true
    width: 1280
    height: 1024

    readonly property string time: "/api/v2/system/time"
    readonly property string services: "/api/v2/system/services/restart"
    readonly property string shotsDir: harness.env("BOXTIME_SHOTS_DIR")

    //main.qml names it: the footer of the page and the keyboard use it
    Item {
        id: rootWindow
        anchors.fill: parent
        property int backs: 0
        function handleBack() { backs++ }
        function goToDesktop() {}
    }

    OverlayLayer {
        id: layer
        objectName: "dialogOverlayLayer"
        //the pages are created after it: without this they take the clicks of the dialogs
        z: 10
    }

    Component {
        id: pageComponent
        ConfigTimeView { anchors.fill: parent }
    }

    function find(item, name) {
        if (item === null || item === undefined)
            return null
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var r = find(item.children[i], name)
            if (r)
                return r
        }
        return null
    }

    function need(name) {
        tryVerify(function() { return find(tc, name) !== null }, 3000, name)
        return find(tc, name)
    }

    function press(name) {
        var it = need(name)
        tryVerify(function() { return it.visible && !it.disabled }, 3000, name + " usable")
        it.buttonClicked()
    }

    function init() {
        harness.reset()
        calaosApp.resetRestarts()
        AppDispatcher.clear()
        rootWindow.backs = 0
    }

    function open() {
        var page = createTemporaryObject(pageComponent, tc)
        verify(page !== null)
        return page
    }

    function openLoaded() {
        var page = open()
        tryVerify(function() { return boxTime.loaded && !boxTime.busy }, 3000)
        return page
    }

    function shot(file) {
        var img = grabImage(tc)
        verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
    }

    function shownMessage() {
        return need("message").text
    }

    function confirmOpened() {
        var dlg = need("confirm")
        tryVerify(function() { return dlg.showing }, 3000)
        //it slides in: a click during the animation lands next to the button
        tryCompare(dlg.anchors, "verticalCenterOffset", 0, 3000)
        return dlg
    }

    function accept(dlg) {
        mouseClick(dlg.positiveButton)
    }

    function refuse(dlg) {
        mouseClick(dlg.negativeButton)
    }

    function backButton(page) {
        for (var i = 0; i < page.footerButtons.length; i++) {
            if (page.footerButtons[i].btClicked)
                return page.footerButtons[i]
        }
        return null
    }

    function changeZone(zone) {
        press("changeZone")
        var picker = need("zonePicker")
        tryVerify(function() { return boxTime.zones.length > 0 }, 3000)
        picker.selected = zone
        press("zoneApply")
    }

    function openWheels() {
        press("setManual")
        var wheels = need("wheels")
        tryVerify(function() { return wheels.year >= 2020 }, 3000)
        return wheels
    }

    function pick(wheels, y, mo, d, h, mi, s) {
        wheels.setValues({ year: y, month: mo, day: d, hour: h, minute: mi, second: s })
        tryVerify(function() {
            return wheels.year === y && wheels.month === mo && wheels.day === d &&
                   wheels.hour === h && wheels.minute === mi && wheels.second === s
        }, 3000, "wheels on the picked value")
    }

    function test_ntpState_data() {
        return [
            { tag: "synchronized", ntp: true, sync: true, text: "Set automatically (NTP), synchronized" },
            { tag: "not yet", ntp: true, sync: false, text: "Set automatically (NTP), not yet synchronized" },
            { tag: "by hand", ntp: false, sync: false, text: "Set by hand" },
            //a free running clock can still report true: it is set by hand all the same
            { tag: "by hand, in sync", ntp: false, sync: true, text: "Set by hand" },
        ]
    }

    function test_ntpState(data) {
        harness.setBox("ntp", data.ntp)
        harness.setBox("synchronized", data.sync)
        openLoaded()
        compare(need("ntpState").text, data.text)
        verify(need("clock").text.indexOf("10:12:03") >= 0, need("clock").text)
        verify(need("date").text.indexOf("2026") >= 0, need("date").text)
    }

    function test_readsAgainAtEachOpening() {
        var page = openLoaded()
        var before = harness.count("GET", time)
        page.destroy()
        wait(0)
        harness.setBox("timezone", "Asia/Tokyo")
        openLoaded()
        verify(harness.count("GET", time) > before)
        tryCompare(boxTime, "timezone", "Asia/Tokyo", 3000)
    }

    function test_nothingShownBeforeTheFirstRead() {
        open()
        verify(find(tc, "stateLoading").visible)
        verify(!find(tc, "clock").visible)
        tryVerify(function() { return boxTime.loaded }, 3000)
    }

    function test_oldDaemonAsksForAnUpdate() {
        harness.setOldDaemon(true)
        var page = open()
        tryVerify(function() { return boxTime.updateRequired }, 3000)
        verify(need("stateUpdate").visible)
        for (var name of ["clock", "changeZone", "toggleNtp", "setManual"])
            verify(!find(tc, name).visible, name)
        //the footer stays usable: the rest of the configuration is one click away
        backButton(page).btClicked()
        compare(rootWindow.backs, 1)
    }

    function test_readFailureCanBeRetried() {
        harness.fail("GET", "/system/time", 500, "internal", "no clock")
        open()
        tryVerify(function() { return find(tc, "stateError").text !== "" }, 3000)
        verify(!find(tc, "clock").visible)
        press("retry")
        tryVerify(function() { return boxTime.loaded }, 3000)
        verify(!find(tc, "stateError").visible)
    }

    function test_zoneList() {
        openLoaded()
        press("changeZone")
        var list = need("zoneList")
        tryCompare(list, "count", 5, 3000)

        //the virtual keyboard answers through the action it was opened with
        press("zoneFilterButton")
        var opened = AppDispatcher.log[AppDispatcher.log.length - 1].message
        verify(typeof opened.returnAction === "function")
        opened.returnAction("eur")
        tryCompare(list, "count", 2, 3000)
        verify(need("zoneFilterClear").visible)

        opened.returnAction("nowhere")
        tryCompare(list, "count", 0, 3000)
        verify(need("zoneNone").visible)

        press("zoneFilterClear")
        tryCompare(list, "count", 5, 3000)
        press("zoneCancel")
        tryVerify(function() { return find(tc, "zonePicker") === null }, 3000)
        compare(harness.count("POST", time), 0)
    }

    function test_applyZoneNeedsAnotherZone() {
        openLoaded()
        press("changeZone")
        var picker = need("zonePicker")
        tryVerify(function() { return boxTime.zones.length > 0 }, 3000)
        verify(need("zoneApply").disabled)
        picker.selected = "Asia/Tokyo"
        verify(!need("zoneApply").disabled)
    }

    function test_changingTheZoneProposesTheRestarts() {
        openLoaded()
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.restartRequired }, 3000)
        compare(JSON.parse(harness.lastBody(time)).timezone, "Asia/Tokyo")
        compare(boxTime.timezone, "Asia/Tokyo")
        verify(need("restartNotice").visible)
        //nothing restarts until a confirmation
        compare(harness.count("POST", services), 0)
        compare(calaosApp.restartCount, 0)
    }

    function test_restartServerAsksFirst() {
        openLoaded()
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.restartRequired }, 3000)

        press("restartServer")
        refuse(confirmOpened())
        wait(50)
        compare(harness.count("POST", services), 0)

        press("restartServer")
        accept(confirmOpened())
        tryVerify(function() { return harness.count("POST", services) === 1 }, 3000)
        compare(JSON.parse(harness.lastBody(services)).unit, "calaos-server")
        tryVerify(function() { return !need("restartServer").visible }, 3000)
        verify(need("restartKiosk").visible)
    }

    function test_restartKioskAsksFirst() {
        openLoaded()
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.restartRequired }, 3000)

        press("restartKiosk")
        var dlg = confirmOpened()
        //the screen goes first: the server would keep the old zone, and the page is gone
        verify(dlg.text.indexOf("has not been restarted") >= 0, dlg.text)
        refuse(dlg)
        wait(50)
        compare(calaosApp.restartCount, 0)

        press("restartKiosk")
        accept(confirmOpened())
        compare(calaosApp.restartCount, 1)
    }

    function test_restartLater() {
        openLoaded()
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.restartRequired }, 3000)
        press("restartLater")
        tryVerify(function() { return !boxTime.restartRequired }, 3000)
        verify(!find(tc, "restartNotice").visible)
        compare(harness.count("POST", services), 0)
        compare(calaosApp.restartCount, 0)
    }

    function test_toggleNtp_data() {
        return [
            { tag: "off", ntp: true, body: { ntp: false } },
            { tag: "on", ntp: false, body: { ntp: true } },
        ]
    }

    function test_toggleNtp(data) {
        harness.setBox("ntp", data.ntp)
        openLoaded()
        press("toggleNtp")
        tryVerify(function() { return boxTime.ntp !== data.ntp && !boxTime.busy }, 3000)
        compare(JSON.parse(harness.lastBody(time)), data.body)
    }

    function test_manualTimeWithNtpIsConfirmed() {
        harness.setBox("now", "2026-07-14T10:00:00Z")
        openLoaded()
        var wheels = openWheels()
        pick(wheels, 2026, 7, 14, 12, 5, 0)
        press("manualSet")

        var dlg = confirmOpened()
        verify(dlg.text.indexOf("will be turned off") >= 0, dlg.text)
        wait(50)
        compare(harness.count("POST", time), 0)

        //the wheels are kept: nothing was sent, the choice can be changed
        refuse(dlg)
        wait(50)
        compare(harness.count("POST", time), 0)
        verify(find(tc, "wheels") !== null)

        press("manualSet")
        accept(confirmOpened())
        tryVerify(function() { return harness.count("POST", time) === 1 }, 3000)
        compare(JSON.parse(harness.lastBody(time)),
                { ntp: false, time: "2026-07-14T12:05:00+02:00" })
        tryVerify(function() { return find(tc, "wheels") === null }, 3000)
    }

    function test_smallStepWithoutNtpIsDirect() {
        harness.setBox("ntp", false)
        harness.setBox("now", "2026-07-14T10:00:00Z")
        openLoaded()
        var wheels = openWheels()
        pick(wheels, 2026, 7, 14, 12, 5, 0)
        press("manualSet")
        tryVerify(function() { return harness.count("POST", time) === 1 }, 3000)
        verify(!find(tc, "confirm").showing)
        compare(JSON.parse(harness.lastBody(time)), { time: "2026-07-14T12:05:00+02:00" })
    }

    function test_bigStepWithoutNtpIsConfirmed() {
        harness.setBox("ntp", false)
        harness.setBox("now", "2026-07-14T10:00:00Z")
        openLoaded()
        var wheels = openWheels()
        pick(wheels, 2026, 7, 14, 14, 0, 0)
        press("manualSet")
        var dlg = confirmOpened()
        verify(dlg.text.indexOf("will move by 2 h") >= 0, dlg.text)
        compare(harness.count("POST", time), 0)
        accept(dlg)
        tryVerify(function() { return harness.count("POST", time) === 1 }, 3000)
    }

    function test_skippedHourIsRefusedOnThePage() {
        harness.setBox("ntp", false)
        harness.setBox("now", "2026-03-29T00:30:00Z")
        openLoaded()
        var wheels = openWheels()
        pick(wheels, 2026, 3, 29, 2, 30, 0)
        press("manualSet")
        tryVerify(function() { return need("manualProblem").text.indexOf("does not exist") >= 0 }, 3000)
        compare(harness.count("POST", time), 0)
        verify(!find(tc, "confirm").showing)

        //the problem goes with the value it was about
        wheels.setValues({ year: 2026, month: 3, day: 29, hour: 3, minute: 30, second: 0 })
        tryVerify(function() { return !find(tc, "manualProblem").visible }, 3000)
    }

    function test_daysFollowTheMonth_data() {
        return [
            { tag: "february", y: 2026, month: 2, last: 28 },
            { tag: "leap february", y: 2028, month: 2, last: 29 },
            { tag: "april", y: 2026, month: 4, last: 30 },
        ]
    }

    function test_daysFollowTheMonth(data) {
        openLoaded()
        var wheels = openWheels()
        pick(wheels, data.y, 1, 31, 12, 0, 0)
        find(tc, "monthWheel").setValue(data.month)
        tryCompare(wheels, "day", data.last, 3000)
        compare(find(tc, "dayWheel").to, data.last)
    }

    function test_theDayComesBackWithALongerMonth() {
        openLoaded()
        var wheels = openWheels()
        pick(wheels, 2026, 1, 31, 12, 0, 0)
        find(tc, "monthWheel").setValue(2)
        tryCompare(wheels, "day", 28, 3000)
        find(tc, "monthWheel").setValue(3)
        tryCompare(wheels, "day", 31, 3000)
    }

    function test_partialFailureIsExplainedThenReadAgain() {
        openLoaded()
        var before = harness.count("GET", time)
        harness.fail("POST", "/system/time", 500, "internal", "ntp failed",
                     { applied: ["timezone"], failed: "ntp" })
        harness.setBox("timezone", "Asia/Tokyo")
        boxTime.setNtp(false)
        tryVerify(function() { return boxTime.message !== "" && !boxTime.busy }, 3000)
        compare(shownMessage(), "Changed: time zone. Not changed: automatic time.")
        verify(boxTime.messageIsError)
        verify(harness.count("GET", time) > before)
        compare(boxTime.timezone, "Asia/Tokyo")
    }

    function test_refusedChangeIsExplained() {
        openLoaded()
        harness.fail("POST", "/system/time", 400, "invalid_request", "unknown timezone")
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.message !== "" && !boxTime.busy }, 3000)
        compare(shownMessage(), "The box does not know this time zone.")
        compare(boxTime.timezone, "Europe/Paris")
    }

    function test_dialogDoesNotOutliveThePage() {
        var page = openLoaded()
        changeZone("Asia/Tokyo")
        tryVerify(function() { return boxTime.restartRequired }, 3000)
        press("restartServer")
        confirmOpened()
        verify(layer.currentOverlay !== null)
        page.destroy()
        tryVerify(function() { return layer.currentOverlay === null }, 3000)
    }

    function test_fitsTheSmallestKiosk_data() {
        return [
            { tag: "main", file: "boxtime-main.png" },
            { tag: "zones", file: "boxtime-zones.png" },
            { tag: "manual", file: "boxtime-manual.png" },
        ]
    }

    function test_fitsTheSmallestKiosk(data) {
        tc.width = 1024
        tc.height = 600
        var page = openLoaded()
        if (data.tag === "zones") {
            press("changeZone")
            tryVerify(function() { return boxTime.zones.length > 0 }, 3000)
        } else if (data.tag === "manual") {
            openWheels()
        }
        wait(100)
        var buttons = data.tag === "zones"? ["zoneCancel", "zoneApply"] :
                      data.tag === "manual"? ["manualCancel", "manualSet"] : []
        for (var name of buttons) {
            var it = need(name)
            var p = it.mapToItem(tc, 0, it.height)
            verify(p.y <= page.footer.y, name + " above the footer")
        }
        shot(data.file)
        tc.width = 1280
        tc.height = 1024
    }
}
