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
            // icon_heater_on.svg is 44x44 natively
            IconItem { id: item; source: "qrc:/img/icon_heater_on.svg" }
        }
    }

    function test_svg_in_layout_keeps_icon_size() {
        var row = createTemporaryObject(rowComponent, tc)
        verify(row)
        tryCompare(row.icon, "status", Image.Ready)
        wait(50)
        compare(row.icon.width, Units.dp(22))
        compare(row.icon.height, Units.dp(22))
    }
}
