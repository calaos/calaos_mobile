import QtQuick
import QtQuick.Controls
import Calaos
import SharedComponents

Item {

    property alias username: userField.text
    property alias password: passField.text
    property alias hostname: hostField.text
    signal loginClicked(var user, var pass, var host)
    signal cancelClicked()

    property color textEditColor: calaosApp.isAndroid? "white": "black"

    visible: opacity > 0?true:false
    Behavior on opacity { PropertyAnimation { } }

    Rectangle {
        color: "#080808"
        opacity: 1
        anchors.fill: parent
    }

    Image {
        id: logo
        source: calaosApp.getPictureSized("logo_full")
        anchors {
            bottom: formbg.top; bottomMargin: Units.dp(20)
            horizontalCenter: formbg.horizontalCenter
        }
    }
    Item {
        id: formbg
        //color: Qt.rgba(0.1, 0.1, 0.1, 1)
        //radius: Units.dp(4)
        smooth: true

        width: isLandscape?Units.dp(300):parent.width - Units.dp(20)
        height: Units.dp(300)

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }

        Text {
            id: ftitle
            text: qsTr("Login to your home")
            color: "#dddddd"
            font { family: calaosFont.fontFamily; bold: true; pointSize: 14 }
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top; topMargin: Units.dp(10)
            }
        }

        Column {
            id: form
            anchors {
                top: parent.top; topMargin: Units.dp(80)
                bottom: parent.bottom; bottomMargin: Units.dp(10)
                //left: parent.left; leftMargin: 10
                //right: parent.right; rightMargin: 10
                horizontalCenter: parent.horizontalCenter
            }

            spacing: Units.dp(32)

            CalaosTextField {
                id: userField
                width: Units.dp(200)
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Username")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { userField.selectAll(); userField.cut() }
            }

            CalaosTextField {
                id: passField
                width: Units.dp(200)
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Password")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { passField.selectAll(); passField.cut() }
            }

            CalaosTextField {
                id: hostField
                width: Units.dp(200)
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Hostname")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { hostField.selectAll(); hostField.cut() }
            }

            ButtonLogin {
                id: loginButton
                text: qsTr("Login")
                width: Units.dp(200)

                onButtonClicked: () => {
                                     if (calaosApp.applicationStatus === Common.Loading) {
                                         cancelClicked()
                                     } else {
                                         loginClicked(userField.text, passField.text, hostField.text)
                                     }
                                 }

                loadingEnabled: calaosApp.applicationStatus === Common.Loading
            }
        }
    }
}
