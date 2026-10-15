import QtQuick
import QtTest
import Calaos
import SharedComponents
import "../../qml/desktop"

//Dialogs are parented to the overlay layer of the window, outside the pages:
//a lost connection must close the ones that show the session, and leave the
//ones about the machine alone.
TestCase {
    id: tc
    name: "OverlayCloseDesktop"
    when: windowShown
    visible: true
    width: 1000
    height: 800

    OverlayLayer {
        id: layer
        objectName: "dialogOverlayLayer"
    }

    Component {
        id: climateComponent
        DialogClimate {}
    }

    Component {
        id: sessionDialog
        Dialog { title: "Session" }
    }

    Component {
        id: machineDialog
        Dialog {
            title: "Machine"
            closeOnDisconnect: false
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

    function init() {
        harness.reset()
        calaosApp.setStatus(Common.LoggedIn)
    }

    function cleanup() {
        calaosApp.setStatus(Common.LoggedIn)
        wait(50)
    }

    function openClimate() {
        var dlg = createTemporaryObject(climateComponent, tc)
        dlg.openFor(makeIo())
        tryVerify(function() { return dlg.detail.showing && dlg.detail.opacity === 1 }, 3000)
        return dlg
    }

    function test_theClimateDialogClosesWhenTheConnectionIsLost() {
        var dlg = openClimate()
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return !dlg.detail.showing && dlg.detail.opacity === 0 }, 3000)
        verify(layer.currentOverlay === null)
    }

    function test_theDialogsStackedOnItCloseToo() {
        var dlg = openClimate()
        wait(350)
        dlg.modeDialog.show()
        dlg.moreDialog.show()
        tryVerify(function() { return dlg.modeDialog.showing && dlg.moreDialog.showing }, 3000)
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return !dlg.detail.showing && !dlg.modeDialog.showing && !dlg.moreDialog.showing }, 3000)
        verify(layer.currentOverlay === null)
    }

    function test_anyDialogClosesNotOnlyTheClimateOnes() {
        var dlg = createTemporaryObject(sessionDialog, tc)
        dlg.show()
        tryVerify(function() { return dlg.showing }, 3000)
        calaosApp.setStatus(Common.Loading)
        tryVerify(function() { return !dlg.showing }, 3000)
    }

    function test_aDialogAboutTheMachineStays() {
        var dlg = createTemporaryObject(machineDialog, tc)
        dlg.show()
        tryVerify(function() { return dlg.showing }, 3000)
        calaosApp.setStatus(Common.NotConnected)
        wait(300)
        verify(dlg.showing)
    }

    function test_aDialogThatIsNotOpenIsLeftAlone() {
        var dlg = createTemporaryObject(sessionDialog, tc)
        var closed = 0
        dlg.closed.connect(function() { closed++ })
        calaosApp.setStatus(Common.NotConnected)
        wait(100)
        compare(closed, 0)
    }

    function test_theDeviceIsReleasedWhenItIsDestroyedBeforeTheDisconnect() {
        var dlg = openClimate()
        harness.destroyIo(dlg.climate.modelData)
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        tryVerify(function() { return dlg.climate.modelData === null }, 3000)
    }
}
