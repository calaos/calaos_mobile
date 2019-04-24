import QtQuick 2.10
import SharedComponents 1.0
import QtQuick.Layouts 1.3
import QtWebEngine 1.7
import QtQuick.VirtualKeyboard 2.2
import QtQuick.VirtualKeyboard.Settings 2.2
import QuickFlux 1.0
import "../quickflux"

Item {
    property bool hideMainMenu: true

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: parent.top
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

    WebEngineView {
        id: webView
        focus: true
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: inputPanel.top
        }
        url: "https://www.google.com"

        AppListener {
            Filter {
                type: ActionTypes.webGoToUrl
                onDispatched: {
                    if (!message.text.startsWith("http://") &&
                        !message.text.startsWith("https://"))
                        message.text = "https://google.com/search?q=" + message.text
                    webView.url = message.text
                }
            }
        }
    }

    Rectangle {
        color: "#3AB4D7"
        height: 2
        anchors {
            left: parent.left
            bottom: webView.bottom
        }
        width: parent.width * webView.loadProgress / 100
        visible: webView.loading
        opacity: 0.7
    }

    Rectangle {
        color: "black"
        anchors {
            left: parent.left; right: parent.right
            top: inputPanel.top; bottom: inputPanel.bottom
        }
    }

    InputPanel {
        id: inputPanel
        anchors {
            bottom: footer.top; bottomMargin: 0
            horizontalCenter: parent.horizontalCenter
        }
        state: "hidden"

        width: parent.width * 0.75

        states: [
            State {
                name: "visible"
                PropertyChanges { target: inputPanel; anchors.bottomMargin: 0 }
            },
            State {
                name: "hidden"
                PropertyChanges { target: inputPanel; anchors.bottomMargin: -inputPanel.implicitHeight }
            }
        ]

        transitions: [
            Transition {
                from: "visible"
                to: "hidden"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "hidden"
                to: "visible"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            }
        ]

        //eats all mouse events to prevent key press when not visible
        MouseArea {
            anchors.fill: parent
            visible: inputPanel.state == "hidden"
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

            RoundButton {
                next: false
                disabled: !webView.canGoBack
                scale: 0.8
                onButtonClicked: webView.goBack()
            }

            RoundButton {
                next: true
                disabled: !webView.canGoForward
                scale: 0.8
                onButtonClicked: webView.goForward()
            }

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: qsTr("Go...")
                icon: "qrc:/img/button_action_plus.png"
                Layout.minimumWidth: width
                onBtClicked: AppActions.openKeyboard(qsTr("URL"),
                                                     qsTr("Enter the url to navigate to"),
                                                     "",
                                                     false,
                                                     ActionTypes.webGoToUrl)
            }

            FooterButton {
                label: qsTr("Keyboard")
                icon: "qrc:/img/button_action_clavier.png"
                Layout.minimumWidth: width
                onBtClicked: inputPanel.state === "visible"? inputPanel.state = "hidden": inputPanel.state = "visible"
            }

            FooterButton {
                label: qsTr("Back to media")
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
}
