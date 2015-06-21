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
        color: "black"
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
    Rectangle {
        id: formbg
        color: "#080808"
        radius: 2 * calaosApp.density
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
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                textColor: "white"

                Image {
                    fillMode: Image.PreserveAspectFit
                    source: calaosApp.getPictureSized("icon_cancel")
                    opacity: parent.text != ""?1:0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { userField.selectAll(); userField.cut() }
                    }
                }
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
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                textColor: "white"

                Image {
                    fillMode: Image.PreserveAspectFit
                    source: calaosApp.getPictureSized("icon_cancel")
                    opacity: parent.text != ""?1:0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { passField.selectAll(); passField.cut() }
                    }
                }
            }

            Text {
                text: qsTr("Host:")
                color: "#dddddd"
                font { bold: false; pointSize: 12 }
            }

            TextField {
                id: hostField
                width: 200 * calaosApp.density
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                textColor: "white"

                Image {
                    fillMode: Image.PreserveAspectFit
                    source: calaosApp.getPictureSized("icon_cancel")
                    opacity: parent.text != ""?1:0
                    Behavior on opacity { NumberAnimation { duration: 100 } }
                    anchors {
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { hostField.selectAll(); hostField.cut() }
                    }
                }
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
