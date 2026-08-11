import QtQuick
import SharedComponents

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
        var numItem = Math.floor(rootWidth / (Units.dp(150)))
        if (numItem > lst.count) numItem = lst.count
        lst.width = numItem * (Units.dp(150))
    }

    onWidthChanged: calcGridSize(width)

    GridView {
        id: lst

        cellHeight: Units.dp(120)
        cellWidth: Units.dp(150)

        anchors.horizontalCenter: parent.horizontalCenter
        height: parent.height - header.height
        y: header.height

        onCountChanged: calcGridSize(parent.width)

        delegate: Item {

            height: Units.dp(120)
            width: Units.dp(150)

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = calaosApp.getPictureSizedPrefix(Calaos.getRoomTypeIcon(roomIconType), "img/rooms")

            Image {
                id: roomIcon
                fillMode: Image.PreserveAspectFit
                width: parent.width - Units.dp(10)
                anchors {
                    centerIn: parent
                    verticalCenterOffset: Units.dp(5)
                }

                Behavior on opacity { PropertyAnimation { duration: 100 } }
            }

            Image {
                id: lighticon
                source: calaosApp.getPictureSized("icon_light_on")

                visible: lights_on_count > 0

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Units.dp(30)
                    bottom: parent.bottom
                    bottomMargin: Units.dp(8)
                }
            }

            Text {
                text: lights_on_count
                visible: lights_on_count > 0
                color: "#ffda5a"
                elide: Text.ElideRight
                anchors {
                    verticalCenter: lighticon.verticalCenter
                    left: lighticon.right; leftMargin: Units.dp(5)
                    right: parent.right; rightMargin: Units.dp(2)
                }
                font { family: calaosFont.fontFamily; bold: false; pointSize: 10 }
            }

            Image {
                id: tempicon
                source: calaosApp.getPictureSized("icon_temp")

                visible: has_temperature

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Units.dp(-55)
                    bottom: parent.bottom
                    bottomMargin: Units.dp(8)
                }
            }

            Text {
                text: "%1°".arg(current_temperature)
                visible: has_temperature
                color: "#3AB4D7"
                elide: Text.ElideRight
                anchors {
                    verticalCenter: tempicon.verticalCenter
                    left: tempicon.right
                }
                font { family: calaosFont.fontFamily; bold: false; pointSize: 10 }
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
                font { family: calaosFont.fontFamily; bold: false; pointSize: 13 }
                text: roomName
                horizontalAlignment: Text.AlignHCenter
                clip: true
                elide: Text.ElideRight
                width: parent.width
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.top; topMargin: Units.dp(5)
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
