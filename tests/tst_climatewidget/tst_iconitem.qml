import QtQuick
import QtQuick.Layouts
import QtTest
import SharedComponents

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
            // 44x44 natively. A raster on purpose: the CI test job has no QtSvg
            // image plugin, and a layout takes the native size of any source.
            IconItem { id: item; source: "qrc:/img/icon_temp@2x.png" }
        }
    }

    function test_icon_in_layout_keeps_icon_size() {
        var row = createTemporaryObject(rowComponent, tc)
        verify(row)
        tryCompare(row.icon, "status", Image.Ready)
        wait(50)
        compare(row.icon.width, Units.dp(22))
        compare(row.icon.height, Units.dp(22))
    }
}
