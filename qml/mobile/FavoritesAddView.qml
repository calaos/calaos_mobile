import QtQuick
import SharedComponents
import Calaos

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
        height: Units.dp(80)

        delegate: Rectangle {

            color: ListView.isCurrentItem?Qt.rgba(0.23, 0.71, 0.84, 0.30):"transparent"

            height: Units.dp(80)
            width: Units.dp(100)

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = calaosApp.getPictureSizedPrefix(CalaosJs.getRoomTypeIcon(roomIconType), "img/rooms")

            Image {
                id: roomIcon
                fillMode: Image.PreserveAspectFit
                width: parent.width - Units.dp(10)
                anchors {
                    centerIn: parent
                    verticalCenterOffset: Units.dp(7)
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
                    top: parent.top; topMargin: Units.dp(2)
                }
            }
        }

    }

    ScrollBar { id: sc1; listObject: roomList; orientation: Qt.Horizontal }
    Rectangle {
        id: sep
        anchors { left: parent.left; right: parent.right; top: sc1.bottom; topMargin: Units.dp(1) }
        color: "#333333"
        height: Units.dp(2)
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

        spacing: Units.dp(5)

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
            border.left: Units.dp(5); border.top: Units.dp(5)
            border.right: Units.dp(5); border.bottom: Units.dp(5)

            width: parent.width - Units.dp(10)
            height: Units.dp(40)

            Text {
                color: "#3ab4d7"
                font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
                text: ioName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: parent.left; leftMargin: Units.dp(8)
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
        height: txt.implicitHeight + Units.dp(36)

        visible: opacity > 0
        opacity: 0
        Behavior on opacity { NumberAnimation {} }

        Text {
            id: txt
            color: "#e7e7e7"
            font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
            text: qsTr("Item added to favorites")
            clip: true
            elide: Text.ElideMiddle
            anchors.centerIn: parent
        }
    }
}
