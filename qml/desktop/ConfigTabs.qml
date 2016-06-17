import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1

BorderImage {

    source: "qrc:/img/controls_bg.png"

    border { left: Units.dp(7); right: Units.dp(7); top: Units.dp(7); bottom: Units.dp(7); }
    horizontalTileMode: BorderImage.Repeat
    verticalTileMode: BorderImage.Repeat

    Column {
        anchors.fill: parent
        anchors.topMargin: Units.dp(8)

        spacing: Units.dp(8)

        Text {
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(18)
            font.family: calaosFont.fontFamily
            font.weight: Font.Thin
            color: Theme.whiteColor

            text: qsTr("System info")
            anchors {
                left: parent.left; leftMargin: Units.dp(10)
                right: parent.right; rightMargin: Units.dp(10)
            }
        }

        Image {
            source: "qrc:/img/controls_sep.png"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Item { width: 1; height: Units.dp(25) }

        ConfigTabLabelValue {
            labelText: qsTr("Product Version:")
            valueText: "-"
        }

        ConfigTabLabelValue {
            labelText: qsTr("System started since:")
            valueText: "-"
        }

        Item { width: 1; height: Units.dp(20) }

        ConfigTabLabelValue {
            labelText: qsTr("Machine name:")
            valueText: "-"
        }

        ConfigTabLabelValue {
            labelText: qsTr("Network address:")
            valueText: "-"
        }

        Item { width: 1; height: Units.dp(20) }

        ConfigTabLabelValue {
            labelText: qsTr("CPU Usage:")
            valueText: "-"
        }

        ConfigTabLabelValue {
            labelText: qsTr("Memory Usage:")
            valueText: "-"
        }
    }

    Image {
        source: "qrc:/img/calaos_about_logo.png"
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: gpl.top; bottomMargin: Units.dp(10)
        }
    }

    Text {
        id: gpl
        anchors {
            left: parent.left; leftMargin: Units.dp(10)
            right: parent.right; rightMargin: Units.dp(10)
            bottom: parent.bottom; bottomMargin: Units.dp(10)
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
