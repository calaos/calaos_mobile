import QtQuick
import QtQuick.Controls as QuickControls
import SharedComponents

SettingsPage {
    id: settingsPage

    title: qsTr("Device")

    //Only what the user typed is saved: an untouched field would otherwise
    //freeze the platform name as an override when the phone is renamed in
    //the system settings while this page is open.
    property bool deviceNameEdited: false

    //Typing does not remove the text binding: without this, a rename
    //reported meanwhile would overwrite what is being typed.
    function markDeviceNameEdited() {
        if (deviceNameEdited)
            return
        deviceNameEdited = true
        deviceNameField.text = deviceNameField.text
    }

    function bindDeviceName() {
        deviceNameField.text = Qt.binding(function() { return hardwareUtils.deviceName })
    }

    //Leaving the page (back, bottom bar, logout, backgrounding) never emits
    //editingFinished: what is typed is saved from here too.
    function commitDeviceName() {
        if (deviceNameField.activeFocus) {
            if (deviceNameField.preeditText !== "")
                markDeviceNameEdited()
            // Qt.inputMethod is seen as a plain QObject by the linter
            // qmllint disable missing-property
            Qt.inputMethod.commit()
            // qmllint enable missing-property
        }
        if (deviceNameEdited) {
            deviceNameEdited = false
            hardwareUtils.setDeviceNameOverride(deviceNameField.text)
        }
        //Still being typed (saved because the app went to the background):
        //left as is under the user's fingers.
        if (!deviceNameField.focus)
            bindDeviceName()
    }

    QuickControls.StackView.onDeactivating: commitDeviceName()
    Component.onDestruction: commitDeviceName()

    //Not on Inactive (iOS Control Center, a system alert): the user comes
    //back to the field. Hidden/Suspended is the app going away.
    Connections {
        target: Qt.application
        function onStateChanged() {
            // qmllint disable missing-property
            if (Qt.application.state === Qt.ApplicationSuspended ||
                Qt.application.state === Qt.ApplicationHidden)
                settingsPage.commitDeviceName()
            // qmllint enable missing-property
        }
    }

    //Sent with register_push: since iOS 16 every iPhone reports the
    //same generic name, the admin needs one that tells them apart.
    ListGroupHeader { width: settingsPage.width; title: qsTr("Name of this device:") }
    Item {
        width: settingsPage.width
        height: Units.dp(48)
        CalaosTextField {
            id: deviceNameField
            objectName: "deviceNameField"
            anchors {
                left: parent.left; leftMargin: Units.dp(16)
                right: parent.right; rightMargin: Units.dp(16)
                verticalCenter: parent.verticalCenter
            }
            text: hardwareUtils.deviceName
            maximumLength: hardwareUtils.deviceNameMaxLength
            EnterKey.type: Qt.EnterKeyDone
            onTextEdited: settingsPage.markDeviceNameEdited()
            onClearButtonClicked: settingsPage.markDeviceNameEdited()
            onAccepted: focus = false
            //Then shows what is stored: sanitized, or the default
            //again when emptied. A window losing activation (iOS
            //Inactive) also ends the editing: not a save.
            onEditingFinished: {
                // qmllint disable missing-property
                if (Qt.application.state === Qt.ApplicationActive)
                    settingsPage.commitDeviceName()
                // qmllint enable missing-property
            }
        }
    }
    Text {
        x: Units.dp(16)
        width: settingsPage.width - Units.dp(32)
        wrapMode: Text.WordWrap
        color: Theme.mutedColor
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(12) }
        text: qsTr("This name lets you recognise this phone in the box administration")
    }

    ListGroupHeader { width: settingsPage.width; title: qsTr("Language:") }

    Repeater {
        model: langModel

        CalaosRadio {
            objectName: "languageRadio"

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
