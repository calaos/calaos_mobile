import QtQuick
import SharedComponents

Dialog {
    id: dlg

    title: qsTr("Reboot?")
    text: qsTr("Do you want to reboot the machine or only the App? A full reboot will do a complete restart of Calaos.")
    hasActions: true
    positiveButtonText: rebootRadio.checked?qsTr("Yes, reboot"):qsTr("Yes, restart")
    negativeButtonText: qsTr("Cancel")

    onAccepted: rebootRadio.checked?calaosApp.rebootMachine():calaosApp.restartApp()

    CalaosRadio {
        id: rebootRadio
        text: qsTr("Reboot machine")
        checked: true
    }
    CalaosRadio {
        text: qsTr("Restart application")
    }
}
