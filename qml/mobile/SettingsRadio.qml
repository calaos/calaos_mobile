import QtQuick
import QtQuick.Controls as Controls
import SharedComponents

//The selection also shows as a check mark, not only as a colour
Controls.RadioButton {
    id: radio

    width: parent ? parent.width : 0
    implicitHeight: Math.max(Units.dp(64), contentItem.implicitHeight + Units.dp(16))
    padding: 0
    leftPadding: Units.dp(12 + 24 + 14)
    rightPadding: Units.dp(12)

    background: Item {
        Rectangle {
            visible: radio.Positioner.index > 0
            width: parent.width
            height: 1
            color: "#424242"
        }
    }

    indicator: Rectangle {
        objectName: "radioIndicator"
        x: Units.dp(12)
        y: (radio.height - height) / 2
        width: Units.dp(24)
        height: width
        radius: width / 2
        color: radio.checked ? Theme.blueColor : "transparent"
        border { width: Units.dp(2); color: radio.checked ? Theme.blueColor : Theme.mutedColor }

        SvgGlyph {
            objectName: "radioCheck"
            anchors.centerIn: parent
            size: Units.dp(16)
            name: "check"
            visible: radio.checked
        }
    }

    contentItem: Text {
        text: radio.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        color: Theme.whiteColor
        font {
            family: calaosFont.fontFamily
            weight: radio.checked ? Font.Normal : Font.Light
            pixelSize: Units.dp(16)
        }
    }
}
