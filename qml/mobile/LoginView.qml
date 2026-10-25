import QtQuick
import QtQuick.Controls
import Calaos
import SharedComponents

Item {
    id: loginRoot

    property alias username: userField.text
    property alias password: passField.text
    property alias hostname: hostField.text
    signal loginClicked(var user, var pass, var host)
    signal cancelClicked()

    property color textEditColor: calaosApp.isAndroid? "white": "black"

    visible: opacity > 0?true:false
    Behavior on opacity { PropertyAnimation { } }

    readonly property bool nativeEntry: hardwareUtils.hasNativePasswordEntry()

    function askNativePassword() {
        hideKeyboard()
        hardwareUtils.requestNativePassword("login", {
            title: qsTr("Sign in"),
            message: "",
            user: userField.text,
            userLabel: qsTr("Username"),
            passwordLabel: qsTr("Password"),
            confirm: qsTr("OK"),
            cancel: qsTr("Cancel")
        })
    }

    Connections {
        target: hardwareUtils
        function onNativePasswordEntered(purpose, user, password) {
            if (purpose !== "login")
                return
            if (user !== "")
                userField.text = user
            passField.text = password
        }
    }

    function hideKeyboard() {
        userField.focus = false
        passField.focus = false
        hostField.focus = false
        // Qt.inputMethod is seen as a plain QObject by the linter
        // qmllint disable missing-property
        Qt.inputMethod.hide()
        // qmllint enable missing-property
    }

    // The fields show the credentials of calaosApp when the view comes on
    // screen, then belong to the user (FIX-8). They are not bound to
    // calaosApp: a binding on a TextField survives typing, so a later change
    // of calaosApp would overwrite what is being typed, and a value that did
    // not change would leave a stale text after a logout. So the fields are
    // refreshed here, explicitly, when the view is created and every time it
    // comes back (logout, session lost, reset), and nothing replaces them
    // while it is on screen.
    function syncFromApp() {
        userField.text = calaosApp.username
        passField.text = calaosApp.password
        hostField.text = calaosApp.hostname
    }

    Component.onCompleted: syncFromApp()

    // Safety net: whatever made the login view leave the screen
    // (login, auto-login), the soft keyboard must not follow us.
    onVisibleChanged: {
        if (visible)
            syncFromApp()
        else
            hideKeyboard()
    }

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
            }

            CalaosTextField {
                id: passField
                width: Units.dp(200)
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Password")
                enabled: !loginButton.loadingEnabled

                //Where the platform has a native credential entry, a tap
                //opens it instead: only real platform fields get AutoFill.
                MouseArea {
                    anchors.fill: parent
                    enabled: loginRoot.nativeEntry && parent.enabled
                    onClicked: loginRoot.askNativePassword()
                }
            }

            CalaosTextField {
                id: hostField
                width: Units.dp(200)
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                placeholderText: qsTr("Hostname")
                enabled: !loginButton.loadingEnabled
            }

            ButtonLogin {
                id: loginButton
                text: qsTr("Login")
                width: Units.dp(200)

                onButtonClicked: () => {
                                     if (calaosApp.applicationStatus === Common.Loading) {
                                         cancelClicked()
                                     } else {
                                         loginRoot.hideKeyboard()
                                         loginClicked(userField.text, passField.text, hostField.text)
                                     }
                                 }

                loadingEnabled: calaosApp.applicationStatus === Common.Loading
            }
        }
    }
}
