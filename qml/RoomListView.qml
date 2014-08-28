import QtQuick 2.2
import "calaos.js" as Calaos;


Item {

    property alias model: lst.model
    property alias visibleArea: lst.visibleArea

    signal roomClicked(int idx, string room_name)

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    function calcGridSize(rootWidth) {
        //calc GridView size to correctly fit the center of the parent
        var numItem = Math.floor(rootWidth / (150 * calaosApp.density))
        if (numItem > lst.count) numItem = lst.count
        lst.width = numItem * (150 * calaosApp.density)
    }

    onWidthChanged: calcGridSize(width)

    GridView {
        id: lst

        cellHeight: 120 * calaosApp.density
        cellWidth: 150 * calaosApp.density

        anchors.horizontalCenter: parent.horizontalCenter
        height: parent.height - header.height
        y: header.height

        onCountChanged: calcGridSize(parent.width)

        delegate: Item {

            height: 120 * calaosApp.density
            width: 150 * calaosApp.density

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = calaosApp.getPictureSizedPrefix(Calaos.getRoomTypeIcon(roomIconType), "img/rooms")

            Image {
                id: roomIcon
                fillMode: Image.PreserveAspectFit
                width: parent.width - 10 * calaosApp.density
                anchors {
                    centerIn: parent
                    verticalCenterOffset: 5 * calaosApp.density
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
                onClicked: roomClicked(index, roomName)
            }

            Text {
                color: "#3ab4d7"
                font { bold: false; pointSize: 13 }
                text: roomName
                horizontalAlignment: Text.AlignHCenter
                clip: true
                elide: Text.ElideRight
                width: parent.width
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.top; topMargin: 5 * calaosApp.density
                }
            }
        }
    }

    ScrollBar { listObject: lst }

    ViewHeader {
        id: header
        headerLabel: qsTr("Home")
        iconSource: calaosApp.getPictureSized("icon_home")
    }
}
