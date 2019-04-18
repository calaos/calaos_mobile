import QtQuick 2.2
import Calaos 1.0
import SharedComponents 1.0
import QuickFlux 1.0
import "../quickflux"

Rectangle {
    id: bg

    property alias mainContent: mainItem.children
    property alias menuContent: menuItem.children

    color: "#272727"

    Rectangle {
        color: "black"
        opacity: 0.4
        anchors.fill: parent
    }

    BorderImage {
        source: "qrc:/img/background_center.png"
        opacity: 0.3

        border {
            left: Units.dp(510)
            right: Units.dp(512)
            top: Units.dp(2)
            bottom: Units.dp(0)
        }

        anchors {
            fill: parent
            bottomMargin: mainItem.anchors.bottomMargin
        }
    }

    BorderImage {
        source: "qrc:/img/background_bottom.png"
        opacity: 0.5

        border {
            left: Units.dp(510)
            right: Units.dp(512)
            top: Units.dp(364)
            bottom: Units.dp(288)
        }

        anchors {
            fill: parent
            bottomMargin: mainItem.anchors.bottomMargin
        }
    }

    BorderImage {
        source: "qrc:/img/background_top.png"
        opacity: 0.3

        border {
            left: Units.dp(510)
            right: Units.dp(512)
            top: Units.dp(364)
            bottom: Units.dp(288)
        }

        anchors {
            fill: parent
            bottomMargin: mainItem.anchors.bottomMargin
        }
    }

    Image {
        source: "qrc:/img/background_quad.png"

        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop

        anchors {
            fill: parent
            bottomMargin: mainItem.anchors.bottomMargin
        }
    }

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
                onDispatched: {
                    mainItem.state = "hidden"
                }
            }

            Filter {
                type: ActionTypes.showMainMenu
                onDispatched: {
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
