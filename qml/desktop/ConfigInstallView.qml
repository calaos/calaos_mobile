import QtQuick
import SharedComponents
import QtQuick.Layouts
import QtQuick.Controls
import QuickFlux
import "../quickflux"

Item {
    id: installer

    property string installDevice

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

        ColumnLayout {

            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                top: parent.top; topMargin: Units.dp(20)
                bottom: parent.bottom; bottomMargin: Units.dp(20)
            }

            spacing: Units.dp(10)

            Text {
                font { family: calaosFont.fontFamily; weight: Font.Medium; pixelSize: Units.dp(20) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                text: qsTr("Calaos OS Installation")
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Text {
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                text: qsTr("Choose a destination disk and click Install to start the installation script")
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

            Text {
                font { family: calaosFont.fontFamily; weight: Font.Normal; pixelSize: Units.dp(15) }
                color: Theme.colorAlpha(Theme.yellowColor, 0.7)
                text: qsTr("WARNING: All data from the destination disk are going to be lost!")
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }


            Repeater {
                model: usbDiskModel

                CalaosRadio {
                    property variant diskItemModel

                    Component.onCompleted: {
                        diskItemModel = Qt.binding(function() { return usbDiskModel.getUsbModel(model.index) })
                    }

                    text: diskItemModel.name + "   (" + diskItemModel.physicalDevice + ")   [" + diskItemModel.sizeHuman + "]"

                    onCheckedChanged: installer.installDevice = diskItemModel.physicalDevice

                    enabled: !osInstaller.isInstalling
                }
            }


            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter

                ListView {
                    id: logView
                    clip: true

                    anchors {
                        fill: parent
                        leftMargin: 4
                        rightMargin: 5
                        topMargin: 4
                        bottomMargin: 4
                    }

                    model: ListModel {}

                    ScrollIndicator.vertical: ScrollIndicator { }

                    delegate: Text {
                        font.family: "Courier New"
                        font.pointSize: 10
                        width: logView.width
                        color: lineColor === "blue"? Theme.blueColor:
                               lineColor === "red"? Theme.redColor:
                               lineColor === "yellow"? Theme.yellowColor:
                               lineColor === "green"? Theme.greenColor: Theme.whiteColor
                        text: line
                    }

                    onCountChanged: logView.positionViewAtEnd()
                }
            }

        }

    }

    AppListener {
        Filter {
            type: ActionTypes.newLogItem
            onDispatched: (filtertype, message) => {
                              logView.model.append({ "line": message.line, "lineColor": message.color })
                          }
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
            text: qsTr("Installation")
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
                label: qsTr("Install")
                icon: "qrc:/img/button_action_valid.png"
                Layout.minimumWidth: width
                visible: !osInstaller.isInstalling
                onBtClicked: {
                    if (installer.installDevice === "") {
                        AppActions.showNotificationMsg(qsTr("Warning"), qsTr("No destination disk selected"), qsTr("Close"))
                    } else {
                        AppActions.hideMainMenu()
                        osInstaller.startInstallation(installer.installDevice)
                    }
                }
            }

            FooterButton {
                id: rebootBt
                visible: osInstaller.installFinished
                label: qsTr("Reboot")
                icon: "qrc:/img/button_action_reload.png"
                Layout.minimumWidth: width
                onBtClicked: AppActions.showRebootDialog(true, false)

                onVisibleChanged: {
                    if (osInstaller.installFinished &&
                        !osInstaller.installError) {
                        AppActions.showRebootDialog(true, false)
                    }
                }
            }

            FooterButton {
                visible: !osInstaller.isInstalling
                label: qsTr("Back to config")
                icon: "qrc:/img/button_action_back.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.handleBack()
            }

            FooterButton {
                visible: !osInstaller.isInstalling
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }

    }
}
