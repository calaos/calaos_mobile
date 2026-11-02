import QtQuick
import QtTest
import SharedComponents
import "../../qml/desktop"

//The screen page of the kiosk: CalaosCheck wraps a long label, a check left at
//its implicit width would then break its label for nothing.
TestCase {
    id: tc
    name: "ConfigScreenView"
    when: windowShown
    visible: true
    width: 1280
    height: 1024

    readonly property string shotsDir: harness.env("BOXTIME_SHOTS_DIR")

    QtObject {
        id: screenManager
        property bool dpmsEnabled: true
        property int dpmsTime: 600000
        function updateDpmsEnabled(on) { dpmsEnabled = on }
        function updateDpmsTime(minutes) {}
    }

    Item {
        id: rootWindow
        anchors.fill: parent
        function handleBack() {}
        function goToDesktop() {}
    }

    Component {
        id: pageComponent
        ConfigScreenView { anchors.fill: parent }
    }

    function find(item, name) {
        if (item.objectName === name)
            return item
        for (var i = 0; i < item.children.length; i++) {
            var r = find(item.children[i], name)
            if (r)
                return r
        }
        return null
    }

    function cleanup() {
        harness.setLanguage("")
    }

    function test_screenOffCheckStaysOnOneLine_data() {
        return [
            { tag: "en", lang: "" },
            { tag: "fr", lang: "fr" },
            { tag: "de", lang: "de" },
        ]
    }

    function test_screenOffCheckStaysOnOneLine(data) {
        verify(harness.setLanguage(data.lang))
        var page = createTemporaryObject(pageComponent, rootWindow)
        verify(page)
        waitForRendering(page)
        var check = find(page, "checkDpms")
        verify(check)
        compare(check.contentItem.lineCount, 1)
        compare(check.height, Units.dp(40))
        if (shotsDir !== "")
            grabImage(tc).save(shotsDir + "/kiosk-screen-" + data.tag + ".png")
    }
}
