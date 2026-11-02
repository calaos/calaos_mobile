import QtQuick
import QtTest
import SharedComponents
import "../../qml/desktop"

//The panel list of the kiosk with its entries translated, and the clock icon.
TestCase {
    id: tc
    name: "ConfigView"
    when: windowShown
    visible: true
    width: 1280
    height: 1024

    readonly property string shotsDir: harness.env("BOXTIME_SHOTS_DIR")

    Rectangle { anchors.fill: parent; color: "#272727" }

    Item {
        id: rootWindow
        anchors.fill: parent
        function handleSubitemClick(id) {}
        function handleBack() {}
        function goToDesktop() {}
    }

    Component {
        id: pageComponent
        ConfigView { anchors.fill: parent }
    }

    function cleanup() {
        harness.setLanguage("")
    }

    function test_frenchPanelList() {
        verify(harness.setLanguage("fr"))
        var page = createTemporaryObject(pageComponent, rootWindow)
        verify(page)
        waitForRendering(page)
        wait(100)
        if (shotsDir !== "")
            grabImage(tc).save(shotsDir + "/config-panel-fr.png")
    }
}
