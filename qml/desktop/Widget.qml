import QtQuick 2.5
import SharedComponents 1.0
import QuickFlux 1.0
import "../quickflux"

Item {
    id: widget

    property variant widgetModel

    x: widgetModel.posX
    y: widgetModel.posY

    width: Math.min(Math.max(widgetModel.width, maximumSize.width), minimumSize.width)
    height: Math.min(Math.max(widgetModel.height, maximumSize.height), minimumSize.height)

    property size minimumSize: Qt.size(20, 20)
    property size maximumSize: Qt.size(500, 500)

    property bool editMode: false

    Drag.active: mouseArea.drag.active

    property int savedX
    property int savedY

    onEditModeChanged: {
        if (editMode) {
            savedX = widget.x
            savedY = widget.y
        }
    }

    ParallelAnimation {
        id: backAnim
        SpringAnimation { id: backAnimX; target: widget; property: "x"; duration: 500; spring: 5; damping: 0.4 }
        SpringAnimation { id: backAnimY; target: widget; property: "y"; duration: 500; spring: 5; damping: 0.4 }
    }

    BorderImage {
        id: editBg
        source: "qrc:/img/widget_back_edit.png"

        border { left: Units.dp(62); right: Units.dp(70); top: Units.dp(65); bottom: Units.dp(64); }
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat

        anchors.fill: parent

        opacity: editMode?1:0
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation {} }

        Image {
            source: "qrc:/img/button_widget_move_left.png"
            anchors {
                right: parent.left
                verticalCenter: parent.verticalCenter
            }

            Image {
                id: arrowLeft
                source: "qrc:/img/button_widget_move_left_big.png"

                anchors {
                    right: parent.right; verticalCenter: parent.verticalCenter
                }

                SequentialAnimation {
                    loops: Animation.Infinite; running: true
                    PropertyAnimation {
                        duration: 200; target: arrowLeft; property: "anchors.rightMargin"; from: 0; to: 5
                    }
                    PropertyAnimation {
                        duration: 200; target: arrowLeft; property: "anchors.rightMargin"; from: 5; to: 0
                    }
                }
            }
        }

        Image {
            source: "qrc:/img/button_widget_move_right.png"
            anchors {
                left: parent.right
                verticalCenter: parent.verticalCenter
            }

            Image {
                id: arrowRight
                source: "qrc:/img/button_widget_move_right_big.png"

                anchors {
                    left: parent.left; verticalCenter: parent.verticalCenter
                }

                SequentialAnimation {
                    loops: Animation.Infinite; running: true
                    PropertyAnimation {
                        duration: 200; target: arrowRight; property: "anchors.leftMargin"; from: 0; to: 5
                    }
                    PropertyAnimation {
                        duration: 200; target: arrowRight; property: "anchors.leftMargin"; from: 5; to: 0
                    }
                }
            }
        }

        Image {
            source: "qrc:/img/button_widget_move_up.png"
            anchors {
                bottom: parent.top
                horizontalCenter: parent.horizontalCenter
            }

            Image {
                id: arrowUp
                source: "qrc:/img/button_widget_move_up_big.png"

                anchors {
                    top: parent.top; horizontalCenter: parent.horizontalCenter
                }

                SequentialAnimation {
                    loops: Animation.Infinite; running: true
                    PropertyAnimation {
                        duration: 200; target: arrowUp; property: "anchors.topMargin"; from: 0; to: 5
                    }
                    PropertyAnimation {
                        duration: 200; target: arrowUp; property: "anchors.topMargin"; from: 5; to: 0
                    }
                }
            }
        }

        Image {
            source: "qrc:/img/button_widget_move_down.png"
            anchors {
                top: parent.bottom
                horizontalCenter: parent.horizontalCenter
            }

            Image {
                id: arrowDown
                source: "qrc:/img/button_widget_move_down_big.png"

                anchors {
                    bottom: parent.bottom; horizontalCenter: parent.horizontalCenter
                }

                SequentialAnimation {
                    loops: Animation.Infinite; running: true
                    PropertyAnimation {
                        duration: 200; target: arrowDown; property: "anchors.bottomMargin"; from: 0; to: 5
                    }
                    PropertyAnimation {
                        duration: 200; target: arrowDown; property: "anchors.bottomMargin"; from: 5; to: 0
                    }
                }
            }
        }
    }

    Loader {
        id: wloader
        anchors.centerIn: parent
        source: widgetModel.moduleSource

        opacity: editMode?0.2:1.0
        Behavior on opacity { PropertyAnimation {} }

        onLoaded: {
            item.widgetModel = widget.widgetModel
            widget.minimumSize = item.minimumSize
            widget.maximumSize = item.maximumSize
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        drag.target: parent
        enabled: editMode
    }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.saveWidgetsPosition
            onDispatched: {
                widgetModel.posX = widget.x
                widgetModel.posY = widget.y
                widgetModel.width = widget.width
                widgetModel.height = widget.height
                widgetsModel.scheduleSave()
            }
        }
        Filter {
            type: ActionTypes.resetWidgetsPosition
            onDispatched: {
                backAnimX.from = widget.x
                backAnimX.to = savedX
                backAnimY.from = widget.y
                backAnimY.to = savedY
                backAnim.start()
            }
        }
    }
}
