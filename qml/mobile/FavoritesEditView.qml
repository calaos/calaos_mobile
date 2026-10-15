import QtQuick
import Calaos
import SharedComponents

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: listViewFav
        model: favoritesModel

        y: header.height
        width: parent.width
        height: parent.height - header.height

        spacing: Units.dp(5)

        delegate: default_delegate
    }

    Text {
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
        anchors.centerIn: listViewFav
        text: qsTr("No favorites yet.<br>Go to <b>Config</b> to add some.")
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        visible: listViewFav.count == 0
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
    }

    ScrollBar { listObject: listViewFav }

    ViewHeader {
        id: header
        headerLabel: qsTr("Edit favorites list")
        iconSource: calaosApp.getPictureSized("fav")
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: calaosApp.getPictureSized("back_items_home")
            border.left: Units.dp(5); border.top: Units.dp(5)
            border.right: Units.dp(5); border.bottom: Units.dp(5)

            width: parent.width - Units.dp(10)
            height: Units.dp(40)

            Item {
                id: icon

                width: Units.dp(20)
                //5 bars of 1dp separated by 4 gaps of 2dp, each rounded like the bars themselves
                height: Units.dp(1) * 5 + Units.dp(2) * 4

                Column {
                    spacing: Units.dp(2)
                    Repeater {
                        model: 5
                        delegate: Rectangle {
                            color: "#C7C7C7"
                            width: Units.dp(20)
                            height: Units.dp(1)
                        }
                    }
                }

                anchors {
                    left: parent.left; leftMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
            }

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: favName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: icon.right; leftMargin: Units.dp(8)
                    right: btadd.left; rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
            }

            ItemButtonAction {
                id: btadd
                anchors {
                    right: parent.right; rightMargin: Units.dp(8)
                    verticalCenter: parent.verticalCenter
                }
                imageSource: "button_min"

                onButtonClicked: {
                    popup.opacity = 1
                    tmr.restart()
                    calaosApp.delItemFavorite(index)
                }
            }

            MouseArea {
                id: dragArea
                anchors.fill: icon
                property int positionStarted: 0
                property int positionEnded: 0
                property int positionsMoved: parent.height > 0 ? Math.floor((positionEnded - positionStarted)/parent.height) : 0
                property int newPosition: index + positionsMoved
                property bool held: false
                drag.axis: Drag.YAxis
                onPressAndHold: {
                    parent.z = 2
                    positionStarted = parent.y
                    dragArea.drag.target = parent
                    parent.opacity = 0.5
                    listViewFav.interactive = false
                    held = true
                    drag.maximumY = (listViewFav.height - parent.height - 1 + listViewFav.contentY)
                    drag.minimumY = 0
                }
                onPositionChanged: {
                    positionEnded = parent.y
                }
                onReleased: {
                    if (Math.abs(positionsMoved) < 1 && held == true) {
                        parent.y = positionStarted
                        parent.opacity = 1
                        listViewFav.interactive = true
                        dragArea.drag.target = null
                        held = false
                    }
                    else {
                        if (held == true) {
                            if (newPosition < 1) {
                                parent.z = 1
                                calaosApp.moveFavorite(index, 0)
                                parent.opacity = 1
                                listViewFav.interactive = true
                                dragArea.drag.target = null
                                held = false
                            }
                            else if (newPosition > listViewFav.count - 1) {
                                parent.z = 1
                                calaosApp.moveFavorite(index, listViewFav.count - 1)
                                parent.opacity = 1
                                listViewFav.interactive = true
                                dragArea.drag.target = null
                                held = false
                            }
                            else {
                                parent.z = 1
                                calaosApp.moveFavorite(index, newPosition)
                                parent.opacity = 1
                                listViewFav.interactive = true
                                dragArea.drag.target = null
                                held = false
                            }
                        }
                    }
                }
            }
        }

    }

    Timer {
        id: tmr
        interval: 1000
        repeat: false
        running: false
        onTriggered: popup.opacity = 0
    }

    Rectangle {
        id: popup
        color: "black"
        anchors {
            left: parent.left; right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: txt.implicitHeight + Units.dp(36)

        visible: opacity > 0
        opacity: 0
        Behavior on opacity { NumberAnimation {} }

        Text {
            id: txt
            color: "#e7e7e7"
            font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
            text: qsTr("Item removed from favorites")
            clip: true
            elide: Text.ElideMiddle
            anchors.centerIn: parent
        }
    }
}
