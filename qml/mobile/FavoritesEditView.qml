import QtQuick 2.0
import Calaos 1.0;
import SharedComponents 1.0

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

        spacing: 5 * calaosApp.density

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
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            width: parent.width - 10 * calaosApp.density
            height: 40 * calaosApp.density

            Item {
                id: icon

                width: 20 * calaosApp.density
                height: (5 * 1 + 4 * 2) * calaosApp.density

                Column {
                    spacing: 2 * calaosApp.density
                    Repeater {
                        model: 5
                        delegate: Rectangle {
                            color: "#C7C7C7"
                            width: 20 * calaosApp.density
                            height: 1 * calaosApp.density
                        }
                    }
                }

                anchors {
                    left: parent.left; leftMargin: 8 * calaosApp.density
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
                    left: icon.right; leftMargin: 8 * calaosApp.density
                    right: btadd.left; rightMargin: 8 * calaosApp.density
                    verticalCenter: parent.verticalCenter
                }
            }

            ItemButtonAction {
                id: btadd
                anchors {
                    right: parent.right; rightMargin: 8 * calaosApp.density
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
                property int positionsMoved: Math.floor((positionEnded - positionStarted)/parent.height)
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
        height: txt.implicitHeight + 36 * calaosApp.density

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
