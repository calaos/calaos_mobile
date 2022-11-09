import QtQuick
import QtQuick.Controls
import Calaos
import SharedComponents

Item {

    property alias username: userField.text
    property alias password: passField.text
    property alias hostname: hostField.text
    signal loginClicked(var user, var pass, var host)

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
            bottom: formbg.top; bottomMargin: 20 * calaosApp.density
            horizontalCenter: formbg.horizontalCenter
        }
    }
    Item {
        id: formbg
        //color: Qt.rgba(0.1, 0.1, 0.1, 1)
        //radius: 4 * calaosApp.density
        smooth: true

        width: isLandscape?300 * calaosApp.density:parent.width - 20 * calaosApp.density
        height: 300 * calaosApp.density

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
                top: parent.top; topMargin: 10 * calaosApp.density
            }
        }

        Column {
            id: form
            anchors {
                top: parent.top; topMargin: 80 * calaosApp.density
                bottom: parent.bottom; bottomMargin: 10 * calaosApp.density
                //left: parent.left; leftMargin: 10
                //right: parent.right; rightMargin: 10
                horizontalCenter: parent.horizontalCenter
            }

            spacing: 32 * calaosApp.density

            CalaosTextField {
                id: userField
                width: 200 * calaosApp.density
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Username")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { userField.selectAll(); userField.cut() }
            }

            CalaosTextField {
                id: passField
                width: 200 * calaosApp.density
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Password")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { passField.selectAll(); passField.cut() }
            }

            CalaosTextField {
                id: hostField
                width: 200 * calaosApp.density
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Hostname")
                enabled: !loginButton.loadingEnabled
                onClearButtonClicked: { hostField.selectAll(); hostField.cut() }
            }

            ButtonLogin {
                id: loginButton
                text: qsTr("Login")
                width: 200 * calaosApp.density

                onButtonClicked: loginClicked(userField.text, passField.text, hostField.text)
                loadingEnabled: calaosApp.applicationStatus === Common.Loading
            }
        }
    }
}
