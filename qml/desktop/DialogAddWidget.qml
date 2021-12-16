import QtQuick
import SharedComponents

Dialog {
    id: dlg

    title: qsTr("Add widget")
    text: qsTr("Select a widget to add to the desktop")
    hasActions: true
    positiveButtonText: qsTr("Add widget")
    negativeButtonText: qsTr("Cancel")

    onAccepted: widgetsModel.addWidget(widgetSelected)

    property string widgetSelected

    Repeater {
        model: widgetsModel.availableWidgets

        CalaosRadio {
            text: modelData
            checked: index == 0
            onCheckedChanged: {
                if (checked) {
                    widgetSelected = modelData
                }
            }
        }
    }
}
