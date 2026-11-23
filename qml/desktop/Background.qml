import QtQuick
import Calaos
import SharedComponents
import QuickFlux
import "../quickflux"
import "../background/qml"

Rectangle {
    id: bg

    property alias mainContent: mainItem.children
    property alias menuContent: menuItem.children

    CalaosBackground { anchors.fill: parent }

    Item {
        id: mainItem

        anchors {
            fill: parent
            topMargin: Units.dp(22)
            bottomMargin: Units.dp(97)
        }

        state: "visible"

        states: [
            State {
                name: "visible"
                PropertyChanges { target: mainItem; anchors.bottomMargin: Units.dp(97) }
            },
            State {
                name: "hidden"
                PropertyChanges { target: mainItem; anchors.bottomMargin: 0 }
            }
        ]

        transitions: [
            Transition {
                from: "visible"
                to: "hidden"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "hidden"
                to: "visible"
                PropertyAnimation { duration: 250; properties: "anchors.bottomMargin"; easing.type: Easing.OutCubic }
            }
        ]

        AppListener {
            Filter {
                type: ActionTypes.hideMainMenu

                onDispatched: (filtertype, message) => {
                    mainItem.state = "hidden"
                }
            }

            Filter {
                type: ActionTypes.showMainMenu
                onDispatched: (filtertype, message) => {
                    mainItem.state = "visible"
                }
            }
        }
    }

    Item {
        id: menuItem
        anchors.fill: parent
    }
}
