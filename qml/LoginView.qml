import QtQuick 2.2
import QtQuick.Controls 1.2

Item {

    property alias username: userField.text
    property alias password: passField.text
    property alias hostname: hostField.text
    signal loginClicked(var user, var pass, var host)

    anchors.fill: parent

    visible: opacity > 0?true:false
    Behavior on opacity { PropertyAnimation { } }

    Rectangle {
        color: "#1f3e47"
        opacity: 0.50
        anchors.fill: parent
    }

    Rectangle {
        color: "black"
        radius: 30
        smooth: true

        width: isLandscape?300:parent.width-20
        height: 300

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: qsTr("Login to your home")
            color: "#dddddd"
            font { bold: true; pointSize: 14 }
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top; topMargin: 10
            }
        }

        Column {
            anchors {
                top: parent.top; topMargin: 50
                bottom: parent.bottom; bottomMargin: 10
                //left: parent.left; leftMargin: 10
                //right: parent.right; rightMargin: 10
                horizontalCenter: parent.horizontalCenter
            }

            spacing: 5

            Text {
                text: qsTr("Login:")
                color: "#dddddd"
                font { bold: false; pointSize: 12 }
            }

            TextField {
                id: userField
                width: 200
                text: "demo@calaos.fr"
            }

            Text {
                text: qsTr("Password:")
                color: "#dddddd"
                font { bold: false; pointSize: 12 }
            }

            TextField {
                id: passField
                width: 200
                echoMode: TextInput.Password
                text: "demo"
            }

            Text {
                text: qsTr("Host:")
                color: "#dddddd"
                font { bold: false; pointSize: 12 }
            }

            TextField {
                id: hostField
                width: 200
                text: "calaos.fr"
            }

            Item { /* spacer */ height: 20; width: 200 }

            Button {
                text: qsTr("Login")
                width: 200
                style: StyleButtonDefault { }

                onClicked: loginClicked(userField.text, passField.text, hostField.text)
            }
        }
    }
}
