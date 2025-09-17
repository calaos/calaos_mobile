import QtQuick
import Calaos
import SharedComponents
import QtQuick.Layouts
import "../quickflux"

Item {
    id: sensorDetailView
    property QtObject sensor

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: flick

        y: header.height
        x: 2 * calaosApp.density
        width: parent.width - x
        height: parent.height - header.height

        contentWidth: mainLayout.width
        contentHeight: mainLayout.height

        ColumnLayout {
            id: mainLayout

            anchors {
                left: parent.left; leftMargin: 16 * calaosApp.density
                right: parent.right; rightMargin: 16 * calaosApp.density
            }

            Item {
                Layout.preferredHeight: Units.dp(8)
                width: 1
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusConnected

                Text {
                    text: qsTr("Connected")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: sensorDetailView.sensor.statusConnected? "#E7E7E7": Theme.redColor
                    text: sensorDetailView.sensor.statusConnected? qsTr("Yes") : qsTr("No")
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusBattLevel

                Text {
                    text: qsTr("Battery level")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: "#E7E7E7"
                    text: sensorDetailView.sensor.statusBattLevel + "%"
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusWirelessSignal

                Text {
                    text: qsTr("Wireless level")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: "#E7E7E7"
                    text: sensorDetailView.sensor.statusWirelessSignal + "%"
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusUptime

                Text {
                    text: qsTr("Uptime")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: "#E7E7E7"
                    text: Calaos.formatTime(sensorDetailView.sensor.statusUptime)
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusIP

                Text {
                    text: qsTr("IP Address")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: "#E7E7E7"
                    text: sensorDetailView.sensor.statusIP
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignLeft

                visible: sensorDetailView.sensor.hasStatusWifiSSID

                Text {
                    text: qsTr("Wifi SSID")
                    font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                    color: Theme.blueColor
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                    color: "#E7E7E7"
                    text: sensorDetailView.sensor.statusWifiSSID
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }

    ScrollBar { listObject: flick }

    ViewHeader {
        id: header
        headerLabel: qsTr("Sensor details")
        iconSource: calaosApp.getPictureSized("icon_sensor")
    }

}
