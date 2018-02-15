import QtQuick 2.5
import QtQuick.Controls 1.3 as QuickControls
import SharedComponents 1.0

Dialog {
    id: dlg

    title: qsTr("Reboot?")
    text: qsTr("Do you want to reboot the machine or only the App? A full reboot will do a complete restart of Calaos.")
    hasActions: true
    positiveButtonText: rebootRadio.checked?qsTr("Yes, reboot"):qsTr("Yes, restart")
    negativeButtonText: qsTr("Cancel")

    onAccepted: rebootRadio.checked?calaosApp.rebootMachine():calaosApp.restartApp()

    QuickControls.ExclusiveGroup {
        id: optionGroup
    }

    CalaosRadio {
        id: rebootRadio
        text: qsTr("Reboot machine")
        checked: true
        exclusiveGroup: optionGroup
    }
    CalaosRadio {
        text: qsTr("Restart application")
        exclusiveGroup: optionGroup
    }
}
