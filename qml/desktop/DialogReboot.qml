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

    function showDialog(showMachine, showApp) {
        rebootRadio.visible = showMachine
        restartRadio.visible = showApp
        //pre-select the machine reboot whenever it is offered, otherwise
        //the app restart: the checked radio drives the accept action, a
        //hidden radio must never stay checked
        rebootRadio.checked = showMachine
        restartRadio.checked = !showMachine
        show()
    }

    CalaosRadio {
        id: rebootRadio
        text: qsTr("Reboot machine")
        checked: true
    }
    CalaosRadio {
        id: restartRadio
        text: qsTr("Restart application")
    }
}
