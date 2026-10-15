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

    //DialogKeyboard sizes itself on the window, as main.qml names it
    Item {
        id: rootWindow
        anchors.fill: parent
    }

    OverlayLayer {
        id: layer
        objectName: "dialogOverlayLayer"
    }


    //Stand-in for the page behind the dialogs
    Rectangle {
        anchors.fill: parent
        color: "#080808"
        z: -1
        Text {
            anchors.centerIn: parent
            text: "Login"
            color: "#3AB4D7"
            font.pixelSize: 40
        }
    }

    readonly property string shotsDir: harness.env("CLIMATE_SHOTS_DIR")

    function shot(file) {
        var img = grabImage(tc)
        verify(img.width > 0)
        if (shotsDir !== "")
            img.save(shotsDir + "/" + file)
    }

    Component {
        id: climateComponent
        DialogClimate {}
    }

    Component {
        id: plainDialog
        Dialog { title: "Plain" }
    }

    Component {
        id: ioDialog
        Dialog {
            title: "Session"
            closeOnDisconnect: true
        }
    }

    Component { id: rebootComponent; DialogReboot {} }
    Component { id: recoveryComponent; DialogRecoveryBoot {} }
    Component { id: netComponent; DialogNetInterface {} }
    Component { id: keyboardComponent; DialogKeyboard {} }
    Component { id: sensorComponent; DialogSensorDetails {} }
    Component { id: colorComponent; DialogRGBColorPicker {} }

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

    function showAndCheck(dlg) {
        dlg.show(0)
        tryVerify(function() { return dlg.showing }, 3000)
    }

    function loseTheConnection() {
        calaosApp.setStatus(Common.NotConnected)
        wait(300)
    }

    function test_aDialogOfTheSessionClosesAndAPlainOneStays() {
        var io = createTemporaryObject(ioDialog, tc)
        var plain = createTemporaryObject(plainDialog, tc)
        showAndCheck(plain)
        showAndCheck(io)
        loseTheConnection()
        verify(!io.showing)
        verify(plain.showing)
    }

    function test_theMachineDialogsStay() {
        var list = [rebootComponent, recoveryComponent, netComponent]
        for (var i = 0; i < list.length; i++) {
            var dlg = createTemporaryObject(list[i], tc)
            showAndCheck(dlg)
            loseTheConnection()
            verify(dlg.showing, "machine dialog " + i)
            dlg.close()
            calaosApp.setStatus(Common.LoggedIn)
        }
    }

    function test_theKeyboardKeepsWhatIsTypedUnlessItIsForAnIo() {
        var net = createTemporaryObject(netComponent, tc)
        showAndCheck(net)
        var kb = createTemporaryObject(keyboardComponent, tc)
        kb.openKeyboard("IP", "address", "192.168.0.1", TextInput.Normal, false, null)
        tryVerify(function() { return kb.showing }, 3000)
        loseTheConnection()
        verify(kb.showing)
        verify(net.showing)
        verify(layer.currentOverlay === kb)
        kb.close()
        verify(layer.currentOverlay === net)

        kb.openKeyboard("Text", "of an io", "x", TextInput.Normal, false, null, true)
        tryVerify(function() { return kb.showing }, 3000)
        loseTheConnection()
        verify(!kb.showing)
        verify(net.showing)
        verify(layer.currentOverlay === net)
    }

    function test_theDialogsOfAnIoClose() {
        var sensor = createTemporaryObject(sensorComponent, tc)
        sensor.showSensor(makeIo())
        tryVerify(function() { return sensor.showing }, 3000)
        var color = createTemporaryObject(colorComponent, tc)
        color.openWithIO(makeIo(), function() {})
        tryVerify(function() { return color.showing }, 3000)
        loseTheConnection()
        verify(!sensor.showing)
        verify(!color.showing)
        verify(layer.currentOverlay === null)
    }

    function test_closingTheTopDialogGivesTheOverlayBackToTheOneBelow() {
        var below = createTemporaryObject(plainDialog, tc)
        var top = createTemporaryObject(ioDialog, tc)
        showAndCheck(below)
        showAndCheck(top)
        verify(layer.currentOverlay === top)
        top.close()
        verify(layer.currentOverlay === below)
        verify(below.showing)
        below.close()
        verify(layer.currentOverlay === null)
    }

    function test_aDialogOfTheSessionOverOneThatStaysLeavesItTheOverlay() {
        var below = createTemporaryObject(plainDialog, tc)
        showAndCheck(below)
        var dlg = openClimate()
        loseTheConnection()
        verify(!dlg.detail.showing)
        verify(below.showing)
        verify(layer.currentOverlay === below)
    }

    function test_theTopDialogClosesFirst() {
        var dlg = openClimate()
        wait(350)
        var order = []
        dlg.detail.closed.connect(function() { order.push("detail") })
        dlg.modeDialog.closed.connect(function() { order.push("mode") })
        dlg.modeDialog.show()
        tryVerify(function() { return dlg.modeDialog.showing }, 3000)
        loseTheConnection()
        compare(order.join(","), "mode,detail")
    }

    function test_theDeviceIsReleasedWhenItIsDestroyedBeforeTheDisconnect() {
        var dlg = openClimate()
        harness.destroyIo(dlg.climate.modelData)
        calaosApp.setStatus(Common.NotConnected)
        tryVerify(function() { return !dlg.detail.showing }, 3000)
        tryVerify(function() { return dlg.climate.modelData === null }, 3000)
    }

    function test_shots() {
        if (shotsDir === "")
            skip("CLIMATE_SHOTS_DIR not set")
        var dlg = openClimate()
        wait(400)
        dlg.moreDialog.show()
        wait(500)
        shot("desktop-open.png")
        calaosApp.setStatus(Common.NotConnected)
        //recorded as it is, whether or not the overlays are gone
        wait(1200)
        shot("desktop-after-disconnect.png")
    }
}
