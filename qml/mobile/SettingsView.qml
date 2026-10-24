import QtQuick
import QtQuick.Controls as QuickControls
import SharedComponents
//import QtQuick.Controls 1.3 as QuickControls
import "../quickflux"

Item {
    id: settingsPage

    signal favoriteAddClicked()
    signal favoriteEditClicked()

    //Leaving the page (back, bottom bar, logout, backgrounding) never emits
    //editingFinished: whatever is typed is saved from here too. The setter
    //is idempotent, and rebinding keeps a later reset from being undone.
    function commitDeviceName() {
        if (deviceNameField.activeFocus) {
            // Qt.inputMethod is seen as a plain QObject by the linter
            // qmllint disable missing-property
            Qt.inputMethod.commit()
            // qmllint enable missing-property
        }
        hardwareUtils.setDeviceNameOverride(deviceNameField.text)
        deviceNameField.text = Qt.binding(function() { return hardwareUtils.deviceName })
    }

    QuickControls.StackView.onDeactivating: commitDeviceName()
    Component.onDestruction: commitDeviceName()

    Connections {
        target: Qt.application
        function onStateChanged() {
            // qmllint disable missing-property
            if (Qt.application.state !== Qt.ApplicationActive)
                settingsPage.commitDeviceName()
            // qmllint enable missing-property
        }
    }

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: listFlick

        Column {
            id: content
            spacing: 6

            ListGroupHeader { width: listFlick.width; title: qsTr("Event log:") }
            Item {
                width: listFlick.width;
                height: Units.dp(40)
                CalaosItemBase {
                    text: qsTr("Show log")
                    width: Units.dp(200)
                    height: parent.height
                    onButtonClicked: AppActions.openEventLog()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Connection:") }
            Item {
                width: listFlick.width;
                height: Units.dp(40)
                CalaosItemBase {
                    text: qsTr("Logout")
                    width: Units.dp(200)
                    height: parent.height
                    onButtonClicked: {
                        settingsPage.commitDeviceName()
                        calaosApp.userLogout()
                    }
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            Item {
                width: listFlick.width;
                height: Units.dp(40)
                CalaosItemBase {
                    text: qsTr("Reset all data")
                    width: Units.dp(200)
                    height: parent.height
                    onButtonClicked: {
                        settingsPage.commitDeviceName()
                        calaosApp.resetAllData()
                    }
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            //Sent with register_push: since iOS 16 every iPhone reports the
            //same generic name, the admin needs one that tells them apart.
            ListGroupHeader { width: listFlick.width; title: qsTr("Name of this device:") }
            Item {
                width: listFlick.width
                height: Units.dp(48)
                CalaosTextField {
                    id: deviceNameField
                    anchors {
                        left: parent.left; leftMargin: Units.dp(16)
                        right: parent.right; rightMargin: Units.dp(16)
                        verticalCenter: parent.verticalCenter
                    }
                    text: hardwareUtils.deviceName
                    maximumLength: hardwareUtils.deviceNameMaxLength
                    EnterKey.type: Qt.EnterKeyDone
                    onAccepted: focus = false
                    //Then shows what is stored: sanitized, or the default
                    //again when emptied.
                    onEditingFinished: settingsPage.commitDeviceName()
                }
            }
            Text {
                x: Units.dp(16)
                width: listFlick.width - Units.dp(32)
                wrapMode: Text.WordWrap
                color: Theme.mutedColor
                font { family: calaosFont.fontFamily; pixelSize: Units.dp(12) }
                text: qsTr("This name lets you recognise this phone in the box administration")
            }

            //Certificates the user trusted (self-signed boxes): forgetting
            //one asks again at the next connection to that host.
            ListGroupHeader {
                width: listFlick.width
                title: qsTr("Trusted certificates:")
                visible: calaosApp.tlsPinList.length > 0
            }

            Repeater {
                model: calaosApp.tlsPinList

                Item {
                    required property var modelData

                    width: listFlick.width
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

            ListGroupHeader { width: listFlick.width; title: qsTr("Favorites:") }

            Item {
                width: listFlick.width;
                height: Units.dp(40)
                CalaosItemBase {
                    text: qsTr("Add a favorite")
                    width: Units.dp(200)
                    height: parent.height
                    onButtonClicked: favoriteAddClicked()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            Item {
                width: listFlick.width;
                height: Units.dp(40)
                CalaosItemBase {
                    text: qsTr("Edit favorites list")
                    width: Units.dp(200)
                    height: parent.height
                    anchors.horizontalCenter: parent.horizontalCenter
                    onButtonClicked: favoriteEditClicked()
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Language:") }

            Repeater {
                model: langModel

                CalaosRadio {
                    property variant langItemModel

                    Component.onCompleted: {
                        langItemModel = Qt.binding(function() { return langModel.getLangModel(model.index) })
                        checked = langItemModel.langActive
                    }

                    text: langItemModel.langName
                    onCheckedChanged: {
                        langItemModel.langActive = checked
                        if (checked) {
                            calaosApp.setLanguage(langItemModel.langCode)
                        }
                    }
                }
            }

        }

        contentHeight: content.implicitHeight
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height
        flickableDirection: Flickable.VerticalFlick
    }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
        headerLabel: qsTr("Settings")
        iconSource: calaosApp.getPictureSized("icon_config")
    }
}
