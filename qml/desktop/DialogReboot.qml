import QtQuick 2.5

Dialog {
    title: qsTr("Reboot?")
    text: qsTr("Do you want to reboot the machine? It will do a complete restart of Calaos.")
    hasActions: true
    positiveButtonText: qsTr("Yes, reboot")
    negativeButtonText: qsTr("Cancel")

    onAccepted: calaosApp.rebootMachine()
}
