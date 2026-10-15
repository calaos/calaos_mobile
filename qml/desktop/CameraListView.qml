import QtQuick
import SharedComponents
import QtQuick.Layouts

PageScaffold {

    title: qsTr("CCTV Camera")

    footerButtons: [
        FooterButton {
            label: qsTr("Back to media")
            icon: "qrc:/img/button_action_back.png"
            Layout.minimumWidth: width
            onBtClicked: rootWindow.handleBack()
        },

        FooterButton {
            label: qsTr("Quit")
            icon: "qrc:/img/button_action_quit.png"
            Layout.minimumWidth: width
            onBtClicked: rootWindow.goToDesktop()
        }
    ]

    Image {
        source: "qrc:/img/neon.png"
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            right: parent.right
        }
    }

    Item {
        id: listviewContainer
        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-80)
        }
        clip: true
        height: Units.dp(300)
        width: Units.dp(218) * 4 + 3 * Units.dp(10)

        ListView {
            id: listview

            Component.onCompleted: cameraModel.cameraVisible = true
            Component.onDestruction: cameraModel.cameraVisible = false

            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: Units.dp(10)
            snapMode: ListView.SnapOneItem

            model: Math.ceil(cameraModel.cameraCount() / 4)
            delegate: Row {
                property int page: index
                height: Units.dp(300)
                spacing: Units.dp(10)
                Repeater {
                    model: 4
                    CameraItem {
                        Component.onCompleted: {
                            var idx = page * 4 + modelData
                            if (idx < cameraModel.cameraCount()) {
                                camModel = Qt.binding(function() {
                                    return cameraModel.getItemModel(idx)
                                })
                                camConnected = true
                            } else {
                                camConnected = false
                            }
                        }
                    }
                }
            }

            SequentialAnimation {
                id: animList
                NumberAnimation { id: animreal; target: listview; property: "contentX"; easing.type: Easing.OutBack; duration: 400 }
                ScriptAction { script: {
                        listview.animMoving = false
                        listview.currentIndex = listview._gotoIndex
                    }
                }
            }
            property int _gotoIndex
            property bool animMoving: false

            function gotoIndexAnimated(idx) {
                _gotoIndex = idx
                var pos = listview.contentX;
                var destPos;
                listview.positionViewAtIndex(idx, ListView.SnapPosition);
                destPos = listview.contentX;
                animreal.from = pos;
                animreal.to = destPos;
                animList.running = true;
                listview.animMoving = true;
            }
        }
    }

    Image {
        source: "qrc:/img/player_line_decoration_right.png"

        opacity: (listview.animMoving || listview.moving) && !listview.atXBeginning?1:0
        Behavior on opacity { PropertyAnimation { duration: 100 } }
        visible: opacity > 0

        anchors {
            top: listviewContainer.top; bottom: listviewContainer.bottom
            right: listviewContainer.left; rightMargin: Units.dp(-14)
        }
    }

    Image {
        source: "qrc:/img/player_line_decoration_left.png"

        opacity: (listview.animMoving || listview.moving) && !listview.atXEnd?1:0
        Behavior on opacity { PropertyAnimation { duration: 100 } }
        visible: opacity > 0

        anchors {
            top: listviewContainer.top; bottom: listviewContainer.bottom
            left: listviewContainer.right; leftMargin: Units.dp(-14)
        }
    }

    RoundButton {
        next: false
        anchors {
            verticalCenter: parent.verticalCenter
            right: listviewContainer.left; rightMargin: Units.dp(-8)
        }
        disabled: listview.atXBeginning
        onButtonClicked: listview.gotoIndexAnimated(listview.currentIndex - 1)
    }

    RoundButton {
        next: true
        anchors {
            verticalCenter: parent.verticalCenter
            left: listviewContainer.right; leftMargin: Units.dp(-8)
        }
        disabled: listview.atXEnd
        onButtonClicked: listview.gotoIndexAnimated(listview.currentIndex + 1)
    }
}
