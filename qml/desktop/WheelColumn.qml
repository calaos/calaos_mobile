import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SharedComponents

//One number on a wheel turned with a finger.
ColumnLayout {
    id: wheel

    property alias title: label.text
    property int from: 0
    property int to: 59
    property int digits: 2

    readonly property int value: from + tumbler.currentIndex

    //the value to keep when the range changes: the Tumbler moves on a new model
    property int wanted: from

    function setValue(newValue) {
        wanted = newValue
        tumbler.currentIndex = newValue - from
    }

    onToChanged: Qt.callLater(function() {
        tumbler.currentIndex = Math.min(wheel.wanted, wheel.to) - wheel.from
    })

    spacing: Units.dp(2)

    Text {
        id: label
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(13) }
        color: Theme.colorAlpha(Theme.whiteColor, 0.7)
        horizontalAlignment: Text.AlignHCenter
        Layout.fillWidth: true
    }

    Tumbler {
        id: tumbler

        objectName: "wheel"
        model: wheel.to - wheel.from + 1
        visibleItemCount: 5

        Layout.fillWidth: true
        Layout.preferredHeight: Units.dp(170)

        onMovingChanged: if (!moving) wheel.wanted = wheel.value

        delegate: Text {
            required property int index

            text: String(wheel.from + index).padStart(wheel.digits, "0")
            font { family: calaosFont.fontFamilyLight; weight: Font.ExtraLight; pixelSize: Units.dp(24) }
            color: index === Tumbler.tumbler.currentIndex? Theme.blueColor : Theme.whiteColor
            opacity: 1.0 - Math.abs(Tumbler.displacement) / (Tumbler.tumbler.visibleItemCount / 2)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height / tumbler.visibleItemCount
            color: "transparent"
            border { color: Theme.colorAlpha(Theme.blueColor, 0.4); width: Units.dp(1) }
            radius: Units.dp(4)
        }
    }
}
