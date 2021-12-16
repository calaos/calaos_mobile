import QtQuick
import SharedComponents
import QtQuick.Layouts

BorderImage {

    source: "qrc:/img/controls_bg.png"

    border { left: Units.dp(7); right: Units.dp(7); top: Units.dp(7); bottom: Units.dp(7); }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat

    Column {
        id: header
        anchors {
            left: parent.left; leftMargin: Units.dp(10)
            right: parent.right; rightMargin: Units.dp(10)
            top: parent.top; topMargin: Units.dp(5)
        }
        spacing: Units.dp(8)

        Text {
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(18)
            font.family: calaosFont.fontFamily
            font.weight: Font.Thin
            color: Theme.whiteColor

            text: qsTr("System info")
            width: parent.width
        }

        Image {
            source: "qrc:/img/controls_sep.png"
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Item {
        anchors{
            top: header.bottom
            bottom: parent.bottom; bottomMargin: Units.dp(2)
            left: parent.left; right: parent.right
        }
        clip: true

        Flickable {
            anchors.fill: parent
            contentHeight: col.implicitHeight
            onContentHeightChanged: console.log("contentheight: " + contentHeight)
            flickableDirection: Flickable.VerticalFlick

            Column {
                id: col
                spacing: Units.dp(8)
                width: parent.width

                Item { width: 1; height: Units.dp(1) }

                ConfigTabLabelValue {
                    labelText: qsTr("Product Version:")
                    valueText: "-"
                }

                ConfigTabLabelValue {
                    id: uptimeTxt
                    labelText: qsTr("System started since:")
                    valueText: "-"

                    Component.onCompleted: {
                        var days = calaosApp.getUptimeDays()
                        if (days == 1) {
                            uptimeTxt.valueText = qsTr("%1 day").arg(days)
                        } else if (days > 1) {
                            uptimeTxt.valueText = qsTr("%1 days").arg(days)
                        }
                    }
                }

                Item { width: 1; height: Units.dp(20) }

                ConfigTabLabelValue {
                    labelText: qsTr("Machine name:")
                    valueText: calaosApp.machineName
                }

                ConfigTabLabelValue {
                    labelText: qsTr("Network:")
                }

                Repeater {
                    model: calaosApp.netAddresses
                    ConfigTabLabelValue {
                        labelText: model.netinterface
                        valueText: model.ipv4
                        small: true
                    }
                }

                Item { width: 1; height: Units.dp(20) }

                ConfigTabProgress {
                    labelText: qsTr("CPU Usage:")
                    valueText: "%1%".arg(calaosApp.cpuUsage)
                    secondValueText: ""
                    progress: calaosApp.cpuUsage
                }

                ConfigTabProgress {
                    labelText: qsTr("Memory Usage:")
                    valueText: "%1%".arg(calaosApp.memoryUsage)
                    secondValueText: ""
                    progress: calaosApp.memoryUsage
                }

                Item { width: 1; height: Units.dp(20) }

                Image {
                    source: "qrc:/img/calaos_about_logo.png"
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                    }
                }

                Text {
                    id: gpl
                    anchors {
                        left: parent.left; leftMargin: Units.dp(10)
                        right: parent.right; rightMargin: Units.dp(10)
                    }

                    wrapMode: Text.Wrap
                    font.pixelSize: Units.dp(12)
                    font.family: calaosFont.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    font.weight: Font.Light
                    color: Theme.colorAlpha(Theme.blueColor, 0.7)

                    text: qsTr("Calaos is free software, you can redistribute it and/or modify it under the terms of the GNU General Public License version 3")
                }

            }
        }

    }
}
