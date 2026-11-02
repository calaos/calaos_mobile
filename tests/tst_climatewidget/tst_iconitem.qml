import QtQuick
import QtQuick.Layouts
import QtTest
import SharedComponents

// No test here depends on the QtSvg plugin: the CI test job does not have it.
TestCase {
    id: tc
    name: "IconItem"
    when: windowShown
    visible: true
    width: 300
    height: 200

    Component {
        id: rowComponent
        RowLayout {
            width: 200
            property alias icon: item
            IconItem { id: item }
        }
    }

    Component {
        id: pngRowComponent
        RowLayout {
            width: 200
            property alias icon: item
            IconItem { id: item; source: "qrc:/img/icon_temp@2x.png" }
        }
    }

    // sourceSize is reset so the 50x50 native size of the raster is the implicit
    // size: only Layout.preferred* then keeps the icon at 22.
    Component {
        id: nativeSizeRowComponent
        RowLayout {
            width: 200
            property alias icon: item
            IconItem { id: item; sourceSize: Qt.size(0, 0); source: "qrc:/img/button_widget_drag.png" }
        }
    }

    function test_layout_keeps_icon_size_whatever_the_implicit_size() {
        var row = createTemporaryObject(nativeSizeRowComponent, tc)
        verify(row)
        tryCompare(row.icon, "status", Image.Ready)
        compare(row.icon.implicitWidth, 50)
        waitForRendering(row)
        compare(row.icon.width, Units.dp(22))
        compare(row.icon.height, Units.dp(22))
    }

    // sourceSize is logical: Qt applies the device pixel ratio itself.
    function test_source_size_is_the_logical_size() {
        var row = createTemporaryObject(rowComponent, tc)
        verify(row)
        compare(row.icon.sourceSize.width, row.icon.width)
        compare(row.icon.sourceSize.height, row.icon.height)
    }

    function test_png_in_layout_keeps_icon_size() {
        var row = createTemporaryObject(pngRowComponent, tc)
        verify(row)
        tryCompare(row.icon, "status", Image.Ready)
        waitForRendering(row)
        compare(row.icon.width, Units.dp(22))
        compare(row.icon.height, Units.dp(22))
    }
}
