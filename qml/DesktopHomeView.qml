import QtQuick 2.2
import "calaos.js" as Calaos;
import Units 1.0
import QtQuick.Layouts 1.1

Item {

    property alias model: lst.model
    property alias visibleArea: lst.visibleArea

    signal roomClicked(int idx, string room_name)

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footer.top
            left: footer.left
            right: footer.right
        }
        opacity: 0.6
    }

    GridView {
        id: lst

        cellHeight: Units.dp(160)
        cellWidth: Units.dp(250)

        clip: true
        snapMode: GridView.SnapToRow

        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter
        height: 3 * cellHeight
        width: 3 * cellWidth
        y: header.height

        flow: GridView.FlowTopToBottom
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            height: lst.cellHeight
            width: lst.cellWidth;

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = qsTr("qrc:/img/rooms/%1/%1_medium.png").arg(Calaos.getRoomTypeIcon(roomIconType))

            Image {
                id: roomIcon
                fillMode: Image.PreserveAspectFit
                anchors {
                    centerIn: parent
                    verticalCenterOffset: Units.dp(8)
                }
            }

            Image {
                id: lighticon
                source: calaosApp.getPictureSized("icon_light_on")

                visible: lights_on_count > 0

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Units.dp(60)
                    bottom: parent.bottom
                    bottomMargin: Units.dp(15)
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
                font { family: calaosFont.fontFamily; bold: false; pointSize: 8 }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onPressed: roomIcon.opacity = 0.5
                onReleased: roomIcon.opacity = 1
                onExited: roomIcon.opacity = 1
                onClicked: roomClicked(index, roomName)
            }

            Text {
                color: "#e7e7e7"
                font { family: calaosFont.fontFamily; bold: true; pointSize: 13 }
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

    Image {
        id: header
        source: "qrc:/img/module_header.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Text {
            anchors.centerIn: parent
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(22)
            font.family: calaosFont.fontFamilyLight
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
            text: qsTr("My Home")
        }
    }

    Image {
        id: footer
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }
}
