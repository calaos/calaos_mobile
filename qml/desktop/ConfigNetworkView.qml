import QtQuick
import SharedComponents
import QtQuick.Layouts

Item {

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footer.top
            left: footer.left
            right: footer.right
        }
        opacity: 0.6
    }

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: tabs.left; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }

            clip: true

            ListView {
                id: list
                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                model: calaosApp.netAddresses

                spacing: Units.dp(10)

                delegate: ItemBase {
                    height: col.implicitHeight + Units.dp(12)

                    ColumnLayout {
                        id: col
                        anchors {
                            leftMargin: Units.dp(5)
                            rightMargin: Units.dp(5)
                            topMargin: Units.dp(6)
                            left: parent.left
                            right: parent.right
                            top: parent.top
                            bottomMargin: Units.dp(6)
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                                text: netinterface
                            }

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                                color: Theme.colorAlpha(Theme.whiteColor, 0.5)
                                text: qsTr("(%1)").arg(isDHCP?"DHCP":"Static")
                            }

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                font { family: calaosFont.fontFamily; weight: Font.Light; pointSize: 12 }
                                color: (netstate == "online" || netstate == "online") ? Theme.greenColor: Theme.redColor
                                text: netstate
                            }

                            ItemButtonAction {
                                Layout.alignment: Qt.AlignVCenter
                                imageSource: "button_empty"
                                iconSource: "qrc:/img/ic_pen.svg"
                                onButtonClicked: {}
                            }
                        }

                        ConfigTabLabelValue {
                            labelText: "IP address:"
                            valueText: ipv4
                            small: true
                            visible: ipv4 !== ""
                        }

                        ConfigTabLabelValue {
                            labelText: "MAC address:"
                            valueText: mac
                            small: true
                            visible: mac !== ""
                        }

                        ConfigTabLabelValue {
                            labelText: "Gateway:"
                            valueText: gateway
                            small: true
                            visible: gateway !== ""
                        }

                        ConfigTabLabelValue {
                            labelText: "DNS:"
                            valueText: dnsServers
                            small: true
                            visible: dnsServers !== ""
                        }

                        ConfigTabLabelValue {
                            labelText: "DNS Search domains:"
                            valueText: searchDomains
                            small: true
                            visible: searchDomains !== ""
                        }
                    }
                }
            }

            ScrollBar { listObject: list }
        }

    }

    ConfigTabs {
        id: tabs
        width: Units.dp(300)

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }
    }

    Image {
        id: header
        source: "qrc:/img/module_header.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Text {
            anchors.centerIn: parent
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(22)
            font.family: calaosFont.fontFamilyLight
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
            text: qsTr("Network configuration")
        }
    }

    Image {
        id: footer
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: qsTr("Refresh")
                icon: "qrc:/img/button_action_reload.png"
                Layout.minimumWidth: width
                onBtClicked: calaosApp.updateNetworkInfo()
            }

            FooterButton {
                label: qsTr("Back to config")
                icon: "qrc:/img/button_action_back.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.handleBack()
            }

            FooterButton {
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }

    }
}
