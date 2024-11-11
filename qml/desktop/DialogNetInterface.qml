import QtQuick
import SharedComponents
import QtQuick.Layouts
import QuickFlux
import "../quickflux"

Dialog {
    id: dlg

    title: qsTr("Configure network interface")
    text: qsTr("Change the network interface configuration")
    hasActions: true
    positiveButtonText: qsTr("Apply")
    negativeButtonText: qsTr("Cancel")

    width: Units.dp(600)

    onAccepted: calaosApp.updateNetwork(intfModel.netinterface, dhcp,
                                        ipText.text, netmaskText.text,
                                        gatewayText.text, dnsText.text, dnsSearchText.text)

    property var intfModel
    property int indexModel
    property bool dhcp

    function show(index) {
        indexModel = index
        intfModel = calaosApp.netAddresses.get(index)
        dhcp = intfModel.isDHCP
        open()
    }

    ColumnLayout {
        id: mainLayout

        anchors {
            left: parent.left; leftMargin: 16 * calaosApp.density
            right: parent.right; rightMargin: 16 * calaosApp.density
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            Text {
                text: qsTr("Interface")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                font { family: calaosFont.fontFamily; weight: Font.Medium; pointSize: 14 }
                color: "#E7E7E7"
                text: intfModel.netinterface
                Layout.alignment: Qt.AlignVCenter
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft

            Text {
                text: qsTr("Mode")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }

            CalaosCombo {
                id: control
                model: ListModel {
                    id: model
                    ListElement { text: "DHCP" }
                    ListElement { text: "Static" }
                }
                currentIndex: intfModel.isDHCP? 0 : 1
                onCurrentIndexChanged: dhcp = control.currentIndex == 0
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            visible: !dhcp

            Text {
                text: qsTr("IPv4")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                id: ipText
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: "#E7E7E7"
                text: intfModel.ipv4
                Layout.alignment: Qt.AlignVCenter
            }
            CalaosButton {
                text: qsTr("Edit")
                hoverEnabled: false
                onButtonClicked: AppActions.openKeyboard(qsTr("Network"),
                                                         qsTr("Enter the new IPv4 address"),
                                                         ipText.text,
                                                         TextInput.Normal,
                                                         false,
                                                         function(text) {
                                                             //check if text is an IP address
                                                             var re = /^(\d{1,3}\.){3}\d{1,3}$/
                                                             if (!re.test(text) && text !== "") {
                                                                 AppActions.showNotificationMsg(qsTr("Validation failed"), qsTr("Invalid IP address"), qsTr("Close"))
                                                                 return
                                                             }
                                                             ipText.text = text
                                                         })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            visible: !dhcp

            Text {
                text: qsTr("Netmask")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                id: netmaskText
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: "#E7E7E7"
                text: intfModel.netmask
                Layout.alignment: Qt.AlignVCenter
            }
            CalaosButton {
                text: qsTr("Edit")
                hoverEnabled: false
                onButtonClicked: AppActions.openKeyboard(qsTr("Network"),
                                                         qsTr("Enter the new IPv4 mask"),
                                                         netmaskText.text,
                                                         TextInput.Normal,
                                                         false,
                                                         function(text) {
                                                             //check if text is an IP address
                                                             var re = /^(\d{1,3}\.){3}\d{1,3}$/
                                                             if (!re.test(text) && text !== "") {
                                                                 AppActions.showNotificationMsg(qsTr("Validation failed"), qsTr("Invalid IP address"), qsTr("Close"))
                                                                 return
                                                             }
                                                             netmaskText.text = text
                                                         })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            visible: !dhcp

            Text {
                text: qsTr("Gateway")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                id: gatewayText
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: "#E7E7E7"
                text: intfModel.gateway
                Layout.alignment: Qt.AlignVCenter
            }
            CalaosButton {
                text: qsTr("Edit")
                hoverEnabled: false
                CalaosButton {
                    text: qsTr("Edit")
                    hoverEnabled: false
                    onButtonClicked: AppActions.openKeyboard(qsTr("Network"),
                                                             qsTr("Enter the new gateway address"),
                                                             gatewayText.text,
                                                             TextInput.Normal,
                                                             false,
                                                             function(text) {
                                                                 //check if text is an IP address
                                                                 var re = /^(\d{1,3}\.){3}\d{1,3}$/
                                                                 if (!re.test(text) && text !== "") {
                                                                     AppActions.showNotificationMsg(qsTr("Validation failed"), qsTr("Invalid IP address"), qsTr("Close"))
                                                                     return
                                                                 }
                                                                 gatewayText.text = text
                                                             })
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            visible: !dhcp

            Text {
                text: qsTr("DNS Servers")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                id: dnsText
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: "#E7E7E7"
                elide: Text.ElideMiddle
                text: intfModel.dnsServers
                Layout.alignment: Qt.AlignVCenter
            }
            CalaosButton {
                text: qsTr("Edit")
                hoverEnabled: false
                CalaosButton {
                    text: qsTr("Edit")
                    hoverEnabled: false
                    onButtonClicked: AppActions.openKeyboard(qsTr("Network"),
                                                             qsTr("Enter the new DNS servers, separated by a comma"),
                                                             dnsText.text,
                                                             TextInput.Normal,
                                                             false,
                                                             function(text) {
                                                                 //check if text is a comma separated list of IP addresses
                                                                 var re = /^(\d{1,3}\.){3}\d{1,3}(,\s*(\d{1,3}\.){3}\d{1,3})*$/
                                                                 if (!re.test(text) && text !== "") {
                                                                     AppActions.showNotificationMsg(qsTr("Validation failed"), qsTr("Invalid IP address list"), qsTr("Close"))
                                                                     return
                                                                 }
                                                                 dnsText.text = text
                                                             })
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignLeft
            visible: !dhcp

            Text {
                text: qsTr("DNS search domains")
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: Theme.blueColor
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
            Text {
                id: dnsSearchText
                font { family: calaosFont.fontFamily; weight: Font.Thin; pointSize: 14 }
                color: "#E7E7E7"
                elide: Text.ElideMiddle
                text: intfModel.searchDomains
                Layout.alignment: Qt.AlignVCenter
            }
            CalaosButton {
                text: qsTr("Edit")
                hoverEnabled: false
                onButtonClicked: AppActions.openKeyboard(qsTr("Network"),
                                                         qsTr("Enter the new DNS search names, separated by a comma"),
                                                         dnsSearchText.text,
                                                         TextInput.Normal,
                                                         false,
                                                         function(text) {
                                                             dnsSearchText.text = text
                                                         })
            }
        }
    }
}
