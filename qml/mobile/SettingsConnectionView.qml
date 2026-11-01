import QtQuick
import SharedComponents

SettingsPage {
    id: connectionPage

    title: qsTr("Connection")

    Item {
        width: connectionPage.width
        height: Units.dp(40)
        CalaosItemBase {
            objectName: "logoutButton"
            text: qsTr("Logout")
            width: Units.dp(200)
            height: parent.height
            onButtonClicked: calaosApp.userLogout()
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }
    Item {
        width: connectionPage.width
        height: Units.dp(40)
        CalaosItemBase {
            objectName: "resetButton"
            text: qsTr("Reset all data")
            width: Units.dp(200)
            height: parent.height
            onButtonClicked: calaosApp.resetAllData()
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    //Certificates the user trusted (self-signed boxes): forgetting
    //one asks again at the next connection to that host.
    ListGroupHeader {
        objectName: "trustedCertificatesHeader"
        width: connectionPage.width
        title: qsTr("Trusted certificates:")
        visible: calaosApp.tlsPinList.length > 0
    }

    Repeater {
        model: calaosApp.tlsPinList

        Item {
            required property var modelData

            width: connectionPage.width
            height: Units.dp(56)

            Column {
                anchors {
                    left: parent.left; leftMargin: Units.dp(16)
                    right: forgetButton.left; rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
                spacing: Units.dp(2)

                Text {
                    width: parent.width
                    elide: Text.ElideMiddle
                    color: Theme.whiteColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(14) }
                    text: modelData.host
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    color: Theme.mutedColor
                    font { family: calaosFont.fontFamily; pixelSize: Units.dp(11) }
                    text: modelData.fingerprint
                }
            }

            CalaosItemBase {
                id: forgetButton
                objectName: "forgetButton"
                text: qsTr("Forget")
                width: Units.dp(90)
                height: Units.dp(40)
                anchors {
                    right: parent.right; rightMargin: Units.dp(16)
                    verticalCenter: parent.verticalCenter
                }
                onButtonClicked: calaosApp.forgetTlsPin(modelData.host)
            }
        }
    }
}
