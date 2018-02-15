import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1

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

            GridView {
                id: gridViewLeft
                model: ListModel {
                    ListElement {
                        titleItem: qsTr("Screen saver")
                        subtitleItem: qsTr("Screen power management")
                        iconItem: "qrc:/img/icon_item_screensaver.png"
                        clickId: "config/screen"
                    }
                }

                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                delegate: ConfigItem {
                    title: titleItem
                    subtitle: subtitleItem
                    icon: iconItem
                    onButtonClicked: rootWindow.handleSubitemClick(clickId)
                }
            }
            ScrollBar { listObject: gridViewLeft }
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
            text: qsTr("Configuration Panel")
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
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }

    }
}
