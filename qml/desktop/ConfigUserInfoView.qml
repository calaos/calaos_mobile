import QtQuick
import SharedComponents
import QtQuick.Layouts
import QuickFlux
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

        RowLayout {
            Image {
                width: Units.dp(32)
                height: Units.dp(32)
                sourceSize: Qt.size(width, height)
                source: calaosApp.getPictureSized("auth_icon")
            }

            Text {
                Layout.fillWidth: true
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(18) }
                color: Theme.whiteColor
                text: qsTr("Change your credentials:")
            }

            Item { Layout.fillWidth: true; height: 1 }

            ItemButtonAction {
                iconSource: calaosApp.settingsLocked? "qrc:/img/ic_locked.svg": "qrc:/img/ic_unlocked.svg"

                onButtonClicked: {
                    if (calaosApp.settingsLocked) {
                        AppActions.openKeyboard(qsTr("Locked"),
                                                qsTr("Enter your password to unlock settings"),
                                                "",
                                                TextInput.Password,
                                                false,
                                                function(txt) {
                                                    if (!calaosApp.unlockSettings(txt)) {
                                                        AppActions.showNotificationMsg(qsTr("Unlock failed"), qsTr("The password you entered is wrong. Please try again."), qsTr("Close"))
                                                    }
                                                }
                        )
                    } else {
                        calaosApp.lockSettings()
                    }
                }
            }
        }

        RowLayout {

            Item { Layout.fillWidth: true; height: 1 }

            CalaosButton {
                text: qsTr("Change username")
                onButtonClicked: AppActions.openKeyboard(qsTr("Username"),
                                                         qsTr("Enter your new username"),
                                                         "",
                                                         TextInput.Normal,
                                                         false,
                                                         function(txt) {
                                                             if (!calaosApp.changeUsername(txt))
                                                                 AppActions.showNotificationMsg(qsTr("Username change failed"), qsTr("The username was not changed. Please try again."), qsTr("Close"))
                                                         })
                hoverEnabled: false
                disabled: calaosApp.settingsLocked
            }

            CalaosButton {
                text: qsTr("Change password")
                onButtonClicked: AppActions.openKeyboard(qsTr("Password"),
                                                         qsTr("Enter your new password"),
                                                         "",
                                                         TextInput.Password,
                                                         false,
                                                         function(pass1) {
                                                             Calaos.singleshotTimer(200, function() {
                                                                 AppActions.openKeyboard(qsTr("Password"),
                                                                                     qsTr("Enter your new password again to validate it"),
                                                                                     "",
                                                                                     TextInput.Password,
                                                                                     false,
                                                                                     function(pass2) {
                                                                                         if (pass1 !== pass2) {
                                                                                             AppActions.showNotificationMsg(qsTr("Password change failed"), qsTr("The two passwords you entered are not the same. Please try again."), qsTr("Close"))
                                                                                             return
                                                                                         }

                                                                                         if (!calaosApp.changePassword(pass2))
                                                                                             AppActions.showNotificationMsg(qsTr("Password change failed"), qsTr("The password was not changed. Please try again."), qsTr("Close"))
                                                                                     })
                                                             })
                                                         })
                hoverEnabled: false
                disabled: calaosApp.settingsLocked
            }
        }

        RowLayout {
            Text {
                Layout.fillWidth: true
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(18) }
                color: Theme.whiteColor
                text: qsTr("Actual username:")
            }

            Item { Layout.fillWidth: true; height: 1 }

            Text {
                Layout.fillWidth: true
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(18) }
                color: Theme.blueColor
                text: calaosApp.username
            }
        }

        Item { Layout.preferredHeight: Units.dp(10); width: 1 }

        Rectangle {
            Layout.preferredHeight: 2
            Layout.fillWidth: true

            color: Theme.colorAlpha(Theme.whiteColor, 0.34)
        }

        Item { Layout.preferredHeight: Units.dp(10); width: 1 }

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
                                                         TextInput.Normal,
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
            onDispatched: (filtertype, message) => {
                console.log("user email append")
                userInfoModel.addEmail(message.text)
            }
        }
    }
}
