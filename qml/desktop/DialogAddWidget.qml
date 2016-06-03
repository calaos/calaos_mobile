import QtQuick 2.5
import QtQuick.Controls 1.3 as QuickControls
import SharedComponents 1.0

Dialog {
    id: dlg

    title: qsTr("Add widget")
    text: qsTr("Select a widget to add to the desktop")
    hasActions: true
    positiveButtonText: qsTr("Add widget")
    negativeButtonText: qsTr("Cancel")

    onAccepted: widgetsModel.addWidget(widgetSelected)

    property string widgetSelected

    QuickControls.ExclusiveGroup {
        id: optionGroup
    }

    Repeater {
        model: widgetsModel.availableWidgets

        CalaosRadio {
            text: modelData
            checked: index == 0
            exclusiveGroup: optionGroup
            onCheckedChanged: {
                if (checked) {
                    widgetSelected = modelData
                }
            }
        }
    }
}
