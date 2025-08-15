import QtQuick
import SharedComponents
import QtQuick.Layouts

Dialog {
    id: dlg

    function showSensor(item) {
        __internal.item = item
        show()
    }

    title: qsTr("Sensor details")
    text: __internal.item ? __internal.item.ioName : ""
    hasActions: true
    positiveButtonEnabled: false
    negativeButtonText: qsTr("Close")

    QtObject {
        id: __internal

        property QtObject item
    }

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

            visible: __internal.item.hasStatusConnected

            Text {
                text: qsTr("Connected")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                color: __internal.item.statusConnected? "#E7E7E7": Theme.redColor
                text: __internal.item.statusConnected? qsTr("Yes") : qsTr("No")
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            visible: __internal.item.hasStatusBattLevel

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
                text: __internal.item.statusBattLevel + "%"
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            visible: __internal.item.hasStatusWirelessSignal

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
                text: __internal.item.statusWirelessSignal + "%"
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            visible: __internal.item.hasStatusUptime

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
                text: Calaos.formatTime(__internal.item.statusUptime)
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            visible: __internal.item.hasStatusIP

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
                text: __internal.item.statusIP
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            visible: __internal.item.hasStatusWifiSSID

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
                text: __internal.item.statusWifiSSID
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
