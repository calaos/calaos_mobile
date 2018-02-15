import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1

Item {

    property alias model: lst.model
    property alias visibleArea: lst.visibleArea

    signal roomClicked(int idx, string room_name, string room_type)

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

        add: Transition {
            NumberAnimation { property: "opacity"; easing.type: Easing.InQuart; from: 0; to: 1; duration: 150 }
        }
        remove: Transition {
            NumberAnimation { property: "opacity"; easing.type: Easing.OutQuart; from: 1; to: 0; duration: 150 }
        }

        cellHeight: (parent.height / 3 ) - header.height
        cellWidth:  (parent.width / 3 )

        clip: true
        snapMode: GridView.SnapToRow

        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter

        height: 3 * cellHeight
        width: 3 * cellWidth
        y: header.height

        flow: GridView.FlowLeftToRight
        boundsBehavior: Flickable.StopAtBounds

        property int page: 0
        onPageChanged: currentIndex = page * 9
        onCurrentIndexChanged: console.log(currentIndex)

        function movePageDown(){
            var p = lst.page
            p++
            if (p > lst.count / 9)
                p = lst.count / 9
            lst.page = p
        }

        function movePageUp() {
            var p = lst.page
            p--
            if (p < 0)
                p = 0
            lst.page = p
        }

        delegate: Item {
            height: lst.cellHeight
            width: lst.cellWidth;

            property string roomIconType: roomType
            onRoomIconTypeChanged: roomIcon.source = "qrc:/img/rooms/%1/%1_medium.png".arg(Calaos.getRoomTypeIcon(roomIconType))

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
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
            }

            Image {
                id: tempicon
                source: calaosApp.getPictureSized("icon_temp")

                visible: has_temperature

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    horizontalCenterOffset: Units.dp(-100)
                    bottom: parent.bottom
                    bottomMargin: Units.dp(15)
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
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onPressed: roomIcon.opacity = 0.5
                onReleased: roomIcon.opacity = 1
                onExited: roomIcon.opacity = 1
                onClicked: roomClicked(index, roomName, roomType)
            }

            Text {
                color: "#e7e7e7"
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
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
        source: "qrc:/img/button_glow_down.png"

        anchors {
            left: parent.left; leftMargin: Units.dp(5)
            bottom: footer.top; bottomMargin: Units.dp(5)
        }

        opacity: lst.atYEnd?0:1
        Behavior on opacity { PropertyAnimation {} }

        MouseArea {
            anchors.fill: parent
            anchors.margins: Units.dp(-5)
            onClicked: lst.movePageDown()
        }
    }

    Image {
        source: "qrc:/img/button_glow_up.png"

        anchors {
            left: parent.left; leftMargin: Units.dp(5)
            top: header.bottom; bottomMargin: Units.dp(5)
        }

        opacity: lst.atYBeginning?0:1
        Behavior on opacity { PropertyAnimation {} }

        MouseArea {
            anchors.fill: parent
            anchors.margins: Units.dp(-5)
            onClicked: lst.movePageUp()
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

        RowLayout {
            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }

    }
}
