import QtQuick 2.5
import SharedComponents 1.0
import QuickFlux 1.0
import "../quickflux"

Item {
    id: widget

    property variant widgetModel

    x: widgetModel.posX
    y: widgetModel.posY
    width: widgetModel.width
    height: widgetModel.height

    property size minimumSize: Qt.size(20, 20)
    property size maximumSize: Qt.size(500, 500)

    property bool editMode: false

    Drag.active: mouseArea.drag.active

    property bool resizeEnabled: minimumSize !== maximumSize

    property int savedX
    property int savedY
    property int savedW
    property int savedH

    onEditModeChanged: {
        if (editMode) {
            savedX = widget.x
            savedY = widget.y
            savedW = widget.width
            savedH = widget.height
        }
    }

    ParallelAnimation {
        id: backAnim
        SpringAnimation { id: backAnimX; target: widget; property: "x"; duration: 500; spring: 5; damping: 0.4 }
        SpringAnimation { id: backAnimY; target: widget; property: "y"; duration: 500; spring: 5; damping: 0.4 }
        SpringAnimation { id: backAnimW; target: widget; property: "width"; duration: 500; spring: 5; damping: 0.4 }
        SpringAnimation { id: backAnimH; target: widget; property: "height"; duration: 500; spring: 5; damping: 0.4 }
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
            visible: resizeEnabled

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

        MouseArea {
            id: mouseAreaLeft
            width: Units.dp(30)
            height: Units.dp(40)
            anchors {
                right: parent.left
                verticalCenter: parent.verticalCenter
            }
            visible: enabled
            enabled: resizeEnabled

            drag { target: mouseAreaLeft; axis: Drag.XAxis }
            onMouseXChanged: {
                if (drag.active) {
                    var oldx = widget.x
                    widget.width = widget.width - mouseX
                    widget.x = widget.x + mouseX

                    if (widget.width < widget.minimumSize.width) {
                        widget.width = widget.minimumSize.width
                        widget.x = oldx
                    }
                    if (widget.width > widget.maximumSize.width) {
                        widget.width = widget.maximumSize.width
                        widget.x = oldx
                    }
                    widgetModel.width = widget.width
                }
            }
        }

        Image {
            visible: resizeEnabled

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

        MouseArea {
            id: mouseAreaRight
            width: Units.dp(30)
            height: Units.dp(40)
            anchors {
                left: parent.right
                verticalCenter: parent.verticalCenter
            }
            visible: enabled
            enabled: resizeEnabled

            drag { target: mouseAreaRight; axis: Drag.XAxis }
            onMouseXChanged: {
                if (drag.active) {
                    widget.width = widget.width + mouseX
                    if (widget.width < widget.minimumSize.width)
                        widget.width = widget.minimumSize.width
                    if (widget.width > widget.maximumSize.width)
                        widget.width = widget.maximumSize.width
                    widgetModel.width = widget.width
                }
            }
        }

        Image {
            visible: resizeEnabled

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

        MouseArea {
            id: mouseAreaTop
            width: Units.dp(40)
            height: Units.dp(30)
            anchors {
                bottom: parent.top
                horizontalCenter: parent.horizontalCenter
            }
            visible: enabled
            enabled: resizeEnabled

            drag { target: mouseAreaTop; axis: Drag.YAxis }
            onMouseYChanged: {
                if (drag.active) {
                    var oldy = widget.y
                    widget.height = widget.height - mouseY
                    widget.y = widget.y + mouseY

                    if (widget.height < widget.minimumSize.height) {
                        widget.height = widget.minimumSize.height
                        widget.y = oldy
                    }
                    if (widget.height > widget.maximumSize.height) {
                        widget.height = widget.maximumSize.height
                        widget.y = oldy
                    }
                    widgetModel.height = widget.height
                }
            }
        }

        Image {
            visible: resizeEnabled

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

        MouseArea {
            id: mouseAreaBottom
            width: Units.dp(40)
            height: Units.dp(30)
            anchors {
                top: parent.bottom
                horizontalCenter: parent.horizontalCenter
            }
            visible: enabled
            enabled: resizeEnabled

            drag { target: mouseAreaRight; axis: Drag.YAxis }
            onMouseYChanged: {
                if (drag.active) {
                    widget.height = widget.height + mouseY
                    if (widget.height < widget.minimumSize.height)
                        widget.height = widget.minimumSize.height
                    if (widget.height > widget.maximumSize.height)
                        widget.height = widget.maximumSize.height
                    widgetModel.height = widget.height
                }
            }
        }

        Image {
            source: "qrc:/img/button_widget_drag.png"
            anchors.centerIn: parent
        }

        Image {
            id: closeBt
            source: "qrc:/img/button_action_del.png"
            anchors {
                top: parent.top
                right: parent.right
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

    MouseArea {
        anchors {
            top: editBg.top; topMargin: Units.dp(-4)
            right: editBg.right; rightMargin: Units.dp(-4)
        }
        //make it a bit bigger
        width: closeBt.width + Units.dp(8)
        height: closeBt.height + Units.dp(8)
        onClicked: dlg.open()
    }

    Dialog {
        id: dlg

        title: qsTr("Remove widget")
        text: qsTr("Remove this widget from the desktop?")
        hasActions: true
        positiveButtonText: qsTr("Yes, remove")
        negativeButtonText: qsTr("No")

        onAccepted: widgetsModel.deleteWidget(widgetModel.uuid)
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
                //Reset the size
                widgetModel.width = savedW
                widgetModel.height = savedH

                backAnimX.from = widget.x
                backAnimX.to = savedX
                backAnimY.from = widget.y
                backAnimY.to = savedY
                backAnimW.from = widget.width
                backAnimW.to = savedW
                backAnimH.from = widget.height
                backAnimH.to = savedH
                backAnim.start()
            }
        }
    }
}
