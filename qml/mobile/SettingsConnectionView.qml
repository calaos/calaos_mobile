import QtQuick
import SharedComponents

SettingsPage {
    id: connectionPage

    title: qsTr("Connection")
    glyph: "link"

    ListGroupHeader { title: qsTr("Session") }

    SettingsRows {
        SettingsRow {
            objectName: "logoutButton"
            title: qsTr("Logout")
            glyph: "logout"
            chevron: true
            onButtonClicked: calaosApp.userLogout()
        }
        SettingsRow {
            objectName: "biometricOffButton"
            title: adminBiometrics.turnOffText
            glyph: "lock"
            visible: adminBiometrics.stored
            onButtonClicked: adminBiometrics.disable()
        }
    }

    ListGroupHeader {
        gap: Units.dp(16)
        danger: true
        title: qsTr("Danger zone")
    }

    SettingsRows {
        danger: true

        SettingsRow {
            objectName: "resetButton"
            title: qsTr("Reset all data")
            glyph: "delete"
            danger: true
            onButtonClicked: calaosApp.resetAllData()
        }
        Text {
            x: Units.dp(66)
            width: parent.width - x - Units.dp(12)
            bottomPadding: Units.dp(12)
            wrapMode: Text.WordWrap
            color: Theme.mutedColor
            font { family: calaosFont.fontFamily; pixelSize: Units.dp(13) }
            text: qsTr("Erases this phone's configuration and certificates.")
        }
    }

    //Certificates the user trusted (self-signed boxes): forgetting
    //one asks again at the next connection to that host.
    ListGroupHeader {
        objectName: "trustedCertificatesHeader"
        gap: Units.dp(16)
        title: qsTr("Trusted certificates:")
        visible: calaosApp.tlsPinList.length > 0
    }

    Repeater {
        model: calaosApp.tlsPinList

        Item {
            required property var modelData

            width: connectionPage.width
            height: Math.max(Units.dp(64), certText.implicitHeight + Units.dp(16))

            Column {
                id: certText
                anchors {
                    left: parent.left; leftMargin: Units.dp(16)
                    right: forgetButton.left; rightMargin: Units.dp(12)
                    verticalCenter: parent.verticalCenter
                }
                spacing: Units.dp(2)

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    color: Theme.whiteColor
                    font { family: calaosFont.fontFamily; pointSize: 12 }
                    text: modelData.host
                }
                Text {
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                    color: Theme.mutedColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(11) }
                    text: modelData.fingerprint
                }
            }

            SettingsButton {
                id: forgetButton
                objectName: "forgetButton"
                small: true
                text: qsTr("Forget")
                anchors {
                    right: parent.right; rightMargin: Units.dp(16)
                    verticalCenter: parent.verticalCenter
                }
                onButtonClicked: calaosApp.forgetTlsPin(modelData.host)
            }

            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: 1
                color: Theme.borderColor
            }
        }
    }
}
