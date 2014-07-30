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
        radius: 30 * calaosApp.density
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
            font { bold: true; pointSize: 14 }
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: parent.top; topMargin: 10 * calaosApp.density
            }
        }

        Column {
            id: form
            anchors {
                top: parent.top; topMargin: 50 * calaosApp.density
                bottom: parent.bottom; bottomMargin: 10 * calaosApp.density
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
                width: 200 * calaosApp.density
                text: "demo@calaos.fr"
            }

            Text {
                text: qsTr("Password:")
                color: "#dddddd"
                font { bold: false; pointSize: 12 }
            }

            TextField {
                id: passField
                width: 200 * calaosApp.density
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
                width: 200 * calaosApp.density
                text: "calaos.fr"
            }

            Item { /* spacer */ height: 20 * calaosApp.density; width: 200 * calaosApp.density }

            Button {
                text: qsTr("Login")
                width: 200 * calaosApp.density
                style: StyleButtonDefault { }

                onClicked: loginClicked(userField.text, passField.text, hostField.text)
            }
        }
    }
}
