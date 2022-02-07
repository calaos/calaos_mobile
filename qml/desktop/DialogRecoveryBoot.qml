import QtQuick
import SharedComponents

Dialog {
    id: dlg

    title: qsTr("Recovery System")
    text: qsTr("The system is booted from a previous snapshot. That means the system is read only and all data will not be saved on the disk.\n\nYou have the ability to rollback to this snapshot as the primary boot system. This rollback procedure will set this snapshot as the current read write system and reboot.\n\nThe rollback option is also available in the Control Panel (Configuration menu)")
    hasActions: true
    dismissOnTap: false
    positiveButtonText: qsTr("Rollback")
    negativeButtonText: qsTr("Close")

    onAccepted: calaosApp.rollbackSnapshot()
}
