import QtQuick 2.0
import SharedComponents 1.0
import Calaos 1.0;

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: roomList
        model: favoritesHomeModel
        orientation: ListView.Horizontal

        y: header.height
        width: parent.width
        height: 80 * calaosApp.density

        delegate: Rectangle {

            color: ListView.isCurrentItem?Qt.rgba(0.23, 0.71, 0.84, 0.30):"transparent"

            height: 80 * calaosApp.density
            width: 100 * calaosApp.density

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = calaosApp.getPictureSizedPrefix(Calaos.getRoomTypeIcon(roomIconType), "img/rooms")

            Image {
                id: roomIcon
                fillMode: Image.PreserveAspectFit
                width: parent.width - 10 * calaosApp.density
                anchors {
                    centerIn: parent
                    verticalCenterOffset: 7 * calaosApp.density
                }

                Behavior on opacity { PropertyAnimation { duration: 100 } }
            }

            MouseArea {
                id: ms
                anchors.fill: parent
                hoverEnabled: true
                onPressed: roomIcon.opacity = 0.5
                onReleased: roomIcon.opacity = 1
                onExited: roomIcon.opacity = 1
                onClicked: roomList.currentIndex = index
            }

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 10 }
                text: roomName
                horizontalAlignment: Text.AlignHCenter
                clip: true
                elide: Text.ElideRight
                width: parent.width
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.top; topMargin: 2 * calaosApp.density
                }
            }
        }

    }

    ScrollBar { id: sc1; listObject: roomList; orientation: Qt.Horizontal }
    Rectangle {
        id: sep
        anchors { left: parent.left; right: parent.right; top: sc1.bottom; topMargin: 1 * calaosApp.density }
        color: "#333333"
        height: 2 * calaosApp.density
    }

    ListView {
        id: itemView

        model: favoritesHomeModel.getRoomModel(roomList.currentIndex)

        clip: true
        anchors {
            top: sep.bottom
            left: parent.left; right: parent.right
            bottom: parent.bottom
        }

        spacing: 5 * calaosApp.density

        delegate: default_delegate
    }

    ScrollBar { listObject: itemView }

    ViewHeader {
        id: header
        headerLabel: qsTr("Add a favorite")
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

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: ioName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: parent.left; leftMargin: 8 * calaosApp.density
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
                imageSource: "button_plus"

                onButtonClicked: {
                    popup.opacity = 1
                    tmr.start()
                    calaosApp.addItemFavorite(ioId, Common.FavIO)
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
            text: "Item added to favorites"
            clip: true
            elide: Text.ElideMiddle
            anchors.centerIn: parent
        }
    }
}
