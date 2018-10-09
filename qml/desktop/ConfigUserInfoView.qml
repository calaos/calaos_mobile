import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3 as QuickControls
import QuickFlux 1.0
import "../quickflux"

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

    ColumnLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(80)
            right: tabs.left; rightMargin: Units.dp(80)
            verticalCenter: parent.verticalCenter
        }

        spacing: Units.dp(10)

        Text {
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
            color: Theme.colorAlpha(Theme.whiteColor, 0.7)
            text: qsTr("Here you can configure some personal information like your email address. It is used by Calaos to send you some email. For example, the Note widget can use those addresses to send notes to your mailbox.")
            wrapMode: Text.WordWrap

            Layout.fillWidth: true
        }

        BorderImage {

            source: "qrc:/img/standard_list_decoration.png"

            border {
                left: Units.dp(27); right: Units.dp(27)
                top: Units.dp(50); bottom: Units.dp(50)
            }

            Layout.minimumHeight: Units.dp(120)
            Layout.maximumHeight: Units.dp(450)
            Layout.preferredHeight: Units.dp(250)
            Layout.fillHeight: true
            Layout.preferredWidth: parent.width - Units.dp(30)

            Item {
                anchors {
                    fill: parent
                    topMargin: Units.dp(2)
                    bottomMargin: Units.dp(2)
                }

                clip: true

                ListView {
                    id: listEmail
                    anchors {
                        fill: parent
                        topMargin: Units.dp(3)
                        bottomMargin: Units.dp(3)
                        leftMargin: Units.dp(5)
                        rightMargin: Units.dp(5)
                    }

                    model: userInfoModel

                    spacing: Units.dp(10)

                    delegate: ItemBase {
                        RowLayout {
                            anchors {
                                leftMargin: Units.dp(5)
                                rightMargin: Units.dp(5)
                                fill: parent
                            }

                            Text {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.fillWidth: true
                                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                                color: "#3ab4d7"
                                text: email
                            }

                            ItemButtonAction {
                                Layout.alignment: Qt.AlignVCenter
                                imageSource: "button_empty"
                                iconSource: "qrc:/img/ic_delete.svg"
                                onButtonClicked: dlgEmailDelete.openForIndex(index, email)
                            }
                        }
                    }
                }

                ScrollBar { listObject: listEmail }
            }
        }

        RowLayout {
            Item { height: 1; Layout.fillWidth: true }

            CalaosButton {
                text: qsTr("Add email")
                onButtonClicked: AppActions.openKeyboard(qsTr("Email"),
                                                         qsTr("Add a new email address to the list"),
                                                         "",
                                                         false,
                                                         ActionTypes.addUserInfoEmail)
                hoverEnabled: false
            }

            Item { height: 1; Layout.preferredWidth: Units.dp(25) }
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
            text: qsTr("User information")
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

    Dialog {
        id: dlgEmailDelete

        title: qsTr("Remove email")
        text: qsTr("Remove this email '%1' from the list?").arg(deleteEmail)
        hasActions: true
        positiveButtonText: qsTr("Yes, remove")
        negativeButtonText: qsTr("No")

        property int deleteIndex
        property string deleteEmail

        function openForIndex(index, email) {
            deleteIndex = index
            deleteEmail = email
            open()
        }

        onAccepted: userInfoModel.deleteEmail(deleteIndex)
    }

    AppListener {
        Filter {
            type: ActionTypes.addUserInfoEmail
            onDispatched: {
                console.log("user email append")
                userInfoModel.addEmail(message.text)
            }
        }
    }
}
