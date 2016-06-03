import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import QuickFlux 1.0
import "../quickflux"

Item {

    property bool editWidgetMode: false

    Repeater {
        model: widgetsModel
        Widget {
            widgetModel: widgetsModel.getWidget(index)
            editMode: editWidgetMode
        }
    }

    function enterEditMode() {
        editWidgetMode = true
        AppActions.hideHomeboardMenu()
    }

    function exitEditMode(save) {
        editWidgetMode = false
        AppActions.showHomeboardMenu()
        if (save) {
            AppActions.saveWidgetsPosition()
        } else {
            AppActions.resetWidgetsPosition()
        }
    }

    BorderImage {
        anchors.fill: parent
        anchors {
            fill: parent
            leftMargin: Units.dp(8); rightMargin: Units.dp(8)
            topMargin: Units.dp(8); bottomMargin: Units.dp(5)
        }

        source: "qrc:/img/widget_container_back.png"
        border { left: Units.dp(207); right: Units.dp(207); top: Units.dp(5); bottom: Units.dp(14); }

        opacity: editWidgetMode?1:0
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation {} }

        Image {
            source: "qrc:/img/widget_container_back_buttons.png"
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
            }

            RowLayout {
                id: l
                anchors {
                    left: parent.left; leftMargin: Units.dp(150)
                    right: parent.right; rightMargin: Units.dp(150)
                    verticalCenter: parent.verticalCenter
                }

                spacing: Units.dp(5)

                FooterButton {
                    label: qsTr("Set position")
                    icon: "qrc:/img/button_action_valid.png"
                    Layout.fillWidth: true
                    onBtClicked: exitEditMode(true)
                }

                FooterButton {
                    label: qsTr("Cancel")
                    icon: "qrc:/img/button_action_quit.png"
                    Layout.fillWidth: true
                    onBtClicked: exitEditMode(false)
                }
            }
        }
    }

    DialogAddWidget { id: dialogAddWidget }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.clickHomeboardItem
            onDispatched: {
                if (message.text == "widgets_add") {
                    dialogAddWidget.show()
                }
                else if (message.text == "widgets_edit") {
                    enterEditMode()
                }
            }
        }
    }
}
