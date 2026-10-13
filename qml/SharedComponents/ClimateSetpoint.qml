import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import "."
import "ClimateLogic.js" as Logic

//One setpoint: a value with -/+ buttons and a slider, or a low/high range with
//a two handles slider. Edits are only reported, the owner decides what to send.
Item {
    id: root

    //Result of ClimateLogic.targetBounds()
    property var bounds: null
    property bool ranged: false
    //Values to show. undefined means the server never reported them.
    property var value
    //{ low, high }, each undefined when the server never reported it. One
    //object so that a move of both handles is applied as one.
    property var range: ({})
    readonly property var low: range ? range.low : undefined
    readonly property var high: range ? range.high : undefined
    property bool active: true

    signal valueEdited(real v)
    signal rangeEdited(var range)

    implicitHeight: col.implicitHeight
    opacity: active ? 1.0 : 0.4
    enabled: active

    //Where the slider and the +/- buttons start from when no value was reported
    property real fallbackValue: bounds ? bounds.min : 0

    readonly property real shownValue: Logic.isNum(value) ? value : fallbackValue

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right }
        spacing: 0

        RowLayout {
            Layout.fillWidth: true

            ItemButtonAction {
                id: btMin
                objectName: "btMin"
                imageSource: "button_min"
                visible: !root.ranged
                onButtonClicked: root.valueEdited(Logic.stepValue(root.shownValue, -1, root.bounds))
            }

            Text {
                id: valueText
                objectName: "valueText"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                color: Theme.yellowColor
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 16 }
                text: root.ranged ? Logic.formatRange(root.low, root.high, root.bounds)
                                  : Logic.formatSetpoint(root.value, root.bounds)
            }

            ItemButtonAction {
                id: btPlus
                objectName: "btPlus"
                imageSource: "button_plus"
                visible: !root.ranged
                onButtonClicked: root.valueEdited(Logic.stepValue(root.shownValue, 1, root.bounds))
            }
        }

        CalaosSlider {
            id: slider
            objectName: "slider"
            Layout.fillWidth: true
            visible: !root.ranged
            from: root.bounds ? root.bounds.min : 0
            to: root.bounds ? root.bounds.max : 1
            stepSize: root.bounds ? root.bounds.step : 1
            snapMode: Controls.Slider.SnapAlways
            value: root.shownValue
            //moved() only fires for a gesture of the user: following the state
            //through the binding above must not echo back as a command
            onMoved: root.valueEdited(Logic.snap(value, root.bounds))
        }

        Controls.RangeSlider {
            id: rangeSlider
            objectName: "rangeSlider"
            Layout.fillWidth: true
            visible: root.ranged
            implicitHeight: Units.dp(40)
            from: root.bounds ? root.bounds.min : 0
            to: root.bounds ? root.bounds.max : 1
            stepSize: root.bounds ? root.bounds.step : 1
            snapMode: Controls.RangeSlider.SnapAlways

            onFromChanged: root.syncHandles()
            onToChanged: root.syncHandles()

            first.onMoved: root.moveHandle("low")
            second.onMoved: root.moveHandle("high")

            background: Item {
                x: rangeSlider.leftPadding
                y: rangeSlider.topPadding + rangeSlider.availableHeight / 2 - height / 2
                width: rangeSlider.availableWidth
                height: Units.dp(4)

                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: Qt.rgba(255, 255, 255, 0.3)

                    Rectangle {
                        x: rangeSlider.first.visualPosition * parent.width
                        width: rangeSlider.second.visualPosition * parent.width - x
                        height: parent.height
                        radius: 2
                        color: Theme.blueColor
                    }
                }
            }

            first.handle: Knob { slider: rangeSlider; handleOf: rangeSlider.first }
            second.handle: Knob { slider: rangeSlider; handleOf: rangeSlider.second }
        }
    }

    function moveHandle(which) {
        //The handle the user did not touch comes from the reported values: the
        //slider may still hold a stale position for it
        var lowIn = which === "low" ? rangeSlider.first.value
                                    : (Logic.isNum(low) ? low : rangeSlider.first.value)
        var highIn = which === "high" ? rangeSlider.second.value
                                      : (Logic.isNum(high) ? high : rangeSlider.second.value)
        var r = Logic.moveRange(lowIn, highIn, which, root.bounds)
        var edit = {}
        //An untouched handle is only sent along when the server reported it:
        //otherwise its slider position is a placeholder.
        if (which === "low" || Logic.isNum(low))
            edit.target_temp_low = r.low
        if (which === "high" || Logic.isNum(high))
            edit.target_temp_high = r.high
        root.rangeEdited(edit)
        syncHandles()
    }

    //Both handles at once: a handle cannot cross the other one, so setting
    //them one by one would leave the first stuck when the range jumps past it.
    //Deferred, so that a burst of changes ends on the last value only.
    function syncHandles() {
        Qt.callLater(applyHandles)
    }

    function applyHandles() {
        if (!bounds)
            return
        rangeSlider.setValues(Logic.isNum(low) ? low : rangeSlider.from,
                              Logic.isNum(high) ? high : rangeSlider.to)
    }

    onRangeChanged: syncHandles()
    Component.onCompleted: applyHandles()

    component Knob: Rectangle {
        id: knob

        required property var slider
        required property var handleOf

        x: slider.leftPadding + handleOf.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        implicitWidth: handleOf.pressed ? Units.dp(26) : Units.dp(18)
        implicitHeight: implicitWidth
        radius: width / 2
        color: Theme.blueColor

        Behavior on implicitWidth { NumberAnimation { duration: 200 } }
    }
}
