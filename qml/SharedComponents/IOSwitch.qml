import QtQuick
import "."
import QtQuick.Layouts
import QuickFlux
import "../quickflux"
import Calaos

ItemBase {
    id: itemBase

    property variant modelData

    property color valueColor
    property string valueText
    property bool blinkEnabled

    onModelDataChanged: modelConnection.onStateChange()

    Connections {
        id: modelConnection
        target: modelData
        ignoreUnknownSignals: true

        function onStateChange() {
            if (modelData.ioType === Common.Switch ||
                modelData.ioType === Common.Switch3 ||
                modelData.ioType === Common.SwitchLong) {
                itemBase.valueColor = modelData.stateBool ? Theme.yellowColor : Theme.whiteColor;
                itemBase.valueText = modelData.stateBool ? qsTr("Active") : qsTr("Inactive");
                itemBase.blinkEnabled = false
            }
            else if (modelData.ioType === Common.DoorSensor ||
                     modelData.ioType === Common.GarageDoorSensor ||
                     modelData.ioType === Common.LockSensor) {
                itemBase.valueColor = modelData.stateBool ? Theme.yellowColor : Theme.whiteColor;
                itemBase.valueText = modelData.stateBool ? qsTr("Closed") : qsTr("Opened");
                itemBase.blinkEnabled = false
            }
            else if (modelData.ioType === Common.OccupancySensor ||
                     modelData.ioType === Common.SoundSensor) {
                itemBase.valueColor = modelData.stateBool ? Theme.yellowColor : Theme.whiteColor;
                itemBase.valueText = modelData.stateBool ? qsTr("Detected") : qsTr("None");
                itemBase.blinkEnabled = false
            }
            else if (modelData.ioType === Common.SmokeSensor ||
                     modelData.ioType === Common.WaterLeakSensor ||
                     modelData.ioType === Common.GasSensor ||
                     modelData.ioType === Common.CO2Sensor ||
                     modelData.ioType === Common.MotionSensor ||
                     modelData.ioType === Common.VibrationSensor) {
                itemBase.valueColor = modelData.stateBool ? Theme.redColor : Theme.whiteColor;
                itemBase.valueText = modelData.stateBool ? qsTr("Detected") : qsTr("None");
                itemBase.blinkEnabled = modelData.stateBool
            }
            else {
                itemBase.valueColor = Theme.whiteColor;
                itemBase.valueText = modelData.stateBool? qsTr("On"): qsTr("Off");
                itemBase.blinkEnabled = false
            }
        }
    }

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        IconItem {
            id: icon

            source: modelData.ioStyle === "switch" ||
                    modelData.ioStyle === "switch3" ||
                    modelData.ioStyle === "switch_long" ? calaosApp.getPictureSized("icon_switch"):
                        calaosApp.getPictureSized("icon_%1".arg(modelData.ioStyle))

            Layout.preferredHeight: height
            Layout.preferredWidth: width
        }

        ScrollingText {
            id: name
            color: "#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.ioName
            clip: true
            Layout.fillWidth: true
        }

        SensorStatusIcon {
            sensor: modelData
        }

        Text {
            id: itemValue
            color: itemBase.valueColor
            font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 12 }
            clip: true
            text: itemBase.valueText
            elide: Text.ElideNone
        }

        SequentialAnimation {
            id: blinkAnimation
            running: itemBase.blinkEnabled
            loops: Animation.Infinite

            NumberAnimation {
                target: itemValue
                property: "opacity"
                from: 1.0
                to: 0.3
                duration: 800
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: itemValue
                property: "opacity"
                from: 0.3
                to: 1.0
                duration: 800
                easing.type: Easing.InOutQuad
            }
        }
    }
}
