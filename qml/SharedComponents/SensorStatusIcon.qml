import QtQuick
import "."
import QuickFlux
import "../quickflux"

MouseArea {
    id: sensorStatusIcon
    width: Units.dp(36)
    height: Units.dp(36)

    property QtObject sensor: null

    enum Status {
        Normal,
        Wireless,
        BatteryLevel,
        BatteryLow,
        Disconnected
    }

    visible: sensorStatusIcon.sensor !== null &&
             sensorStatusIcon.sensor.hasStatusInfo

    property int status: sensorStatusIcon.sensor.hasStatusConnected &&
                         sensorStatusIcon.sensor.statusConnected === false ? SensorStatusIcon.Status.Disconnected :
                         sensorStatusIcon.sensor.hasStatusBattLevel &&
                         sensorStatusIcon.sensor.statusBattLevel <= 30 ? SensorStatusIcon.Status.BatteryLow :
                         sensorStatusIcon.sensor.hasStatusBattLevel &&
                         sensorStatusIcon.sensor.statusBattLevel > 30 ? SensorStatusIcon.Status.BatteryLevel :
                         sensorStatusIcon.sensor.hasStatusWirelessSignal ? SensorStatusIcon.Status.Wireless :
                         SensorStatusIcon.Normal

    onClicked: {
        if (sensorStatusIcon.visible) {
            AppActions.showSensorDetails(sensorStatusIcon.sensor)
        }
    }

    IconItem {
        id: iconItem
        anchors.centerIn: parent
        source: {
            switch(sensorStatusIcon.status) {
                case SensorStatusIcon.Status.BatteryLow:
                    return calaosApp.getPictureSized("icon_battery_empty")
                case SensorStatusIcon.Status.Disconnected:
                    return calaosApp.getPictureSized("icon_wifi_off")
                case SensorStatusIcon.Status.Wireless:
                {
                    if (sensorStatusIcon.sensor.statusWirelessSignal >= 75) {
                        return calaosApp.getPictureSized("icon_wifi_100")
                    } else if (sensorStatusIcon.sensor.statusWirelessSignal >= 50) {
                        return calaosApp.getPictureSized("icon_wifi_75")
                    } else if (sensorStatusIcon.sensor.statusWirelessSignal >= 25) {
                        return calaosApp.getPictureSized("icon_wifi_50")
                    }

                    return calaosApp.getPictureSized("icon_wifi_25")
                }

                case SensorStatusIcon.Status.BatteryLevel:
                {
                    if (sensorStatusIcon.sensor.statusBattLevel >= 75) {
                        return calaosApp.getPictureSized("icon_battery_full")
                    } else if (sensorStatusIcon.sensor.statusBattLevel >= 50) {
                        return calaosApp.getPictureSized("icon_battery_75")
                    } else if (sensorStatusIcon.sensor.statusBattLevel >= 25) {
                        return calaosApp.getPictureSized("icon_battery_25")
                    }

                    return calaosApp.getPictureSized("icon_battery_empty")
                }

                default:
                    return calaosApp.getPictureSized("icon_sensor")
            }
        }

        SequentialAnimation {
            id: blinkAnimation
            running: sensorStatusIcon.status === SensorStatusIcon.Status.BatteryLow ||
                     sensorStatusIcon.status === SensorStatusIcon.Status.Disconnected
            loops: Animation.Infinite

            NumberAnimation {
                target: iconItem
                property: "opacity"
                from: 1.0
                to: 0.3
                duration: 800
                easing.type: Easing.InOutQuad
            }

            NumberAnimation {
                target: iconItem
                property: "opacity"
                from: 0.3
                to: 1.0
                duration: 800
                easing.type: Easing.InOutQuad
            }
        }
    }
}
