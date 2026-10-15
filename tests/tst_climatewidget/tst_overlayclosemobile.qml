import QtQuick
import QtQuick.Controls
import QtTest
import Calaos
import SharedComponents
import "../../qml/mobile"

//The app falls back to the login page when the connection is lost: nothing
//opened over the logged-in pages may stay on screen.
TestCase {
    id: tc
    name: "OverlayCloseMobile"
    when: windowShown
    visible: true
    width: 420
    height: 900

    Component {
        id: hostComponent
        ClimateSheet {}
    }

    Component {
        id: plainSheet
        BottomSheet {
            title: "Plain"
            heightRatio: 0.5
        }
    }

    function device() {
        return { family: "climate", impl: "proxy",
                 capabilities: { modes: ["off", "heat", "cool"],
                                 target_temp: { min: 16, max: 30, step: 0.5, unit: "°C" },
                                 fan_modes: ["auto", "low"], reports_action: true },
                 state: { mode: "cool", current_temp: 25, target_temp: 22, action: "cooling", fan_mode: "auto" },
                 ios: { mode: "m", target_temp: "t" } }
    }

    function makeIo() {
        return harness.makeClimate({ id: "salon_ac", name: "Clim salon", gui_type: "climate",
                                     state: "cool", rw: "true", device: device() })
    }

    function openHost() {
        var host = createTemporaryObject(hostComponent, tc)
        host.openFor(makeIo())
        tryVerify(function() { return host.opened }, 3000)
        return host
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

    function openModeAndMore(host) {
        tryVerify(function() { return find(Overlay.overlay, "modePill") !== null && find(Overlay.overlay, "modePill").width > 0 }, 3000)
        wait(300)
        find(Overlay.overlay, "modePill").buttonClicked()
        tryVerify(function() { return host.modeOpened }, 3000)
        wait(300)
        find(Overlay.overlay, "moreLink").clicked(null)
        tryVerify(function() { return host.moreOpened }, 3000)
    }

    function allClosed(host) {
        return !host.opened && !host.modeOpened && !host.moreOpened
    }

    function init() {
        harness.reset()
        calaosApp.setStatus(Common.LoggedIn)
    }

    function cleanup() {
        calaosApp.setStatus(Common.LoggedIn)
        wait(50)
    }

    function test_theSheetClosesWhenTheConnectionIsLost() {
        var host = openHost()
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return allClosed(host) }, 3000)
    }

    function test_theSheetsStackedOnItCloseToo() {
        var host = openHost()
        openModeAndMore(host)
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return allClosed(host) }, 3000)
    }

    function test_anySheetClosesNotOnlyTheClimateOnes() {
        var sheet = createTemporaryObject(plainSheet, tc)
        sheet.open()
        tryVerify(function() { return sheet.opened }, 3000)
        calaosApp.setStatus(Common.Loading)
        tryVerify(function() { return !sheet.opened && !sheet.visible }, 3000)
    }

    function test_aSheetOpenedAgainAfterLoginStays() {
        var host = openHost()
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return allClosed(host) }, 3000)
        calaosApp.setStatus(Common.LoggedIn)
        host.openFor(makeIo())
        tryVerify(function() { return host.opened }, 3000)
        wait(200)
        verify(host.opened)
    }

    function test_theSheetLetsGoOfADeviceDestroyedBeforeTheDisconnect() {
        var host = openHost()
        tryVerify(function() { return find(Overlay.overlay, "modePill") !== null && find(Overlay.overlay, "modePill").width > 0 }, 3000)
        wait(300)
        find(Overlay.overlay, "modePill").buttonClicked()
        tryVerify(function() { return host.modeOpened }, 3000)
        harness.destroyIo(host.climate.modelData)
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return allClosed(host) }, 3000)
        tryVerify(function() { return host.climate.modelData === null }, 3000)
        wait(100)
        compare(host.climate.name, "")
    }

    function test_aDeviceDestroyedWhileTheSheetIsOpenDoesNotBreakIt() {
        var host = openHost()
        harness.destroyIo(host.climate.modelData)
        wait(100)
        host.close()
        tryVerify(function() { return allClosed(host) }, 3000)
    }
}
