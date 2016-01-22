import QtQuick 2.2
import "calaos.js" as Calaos;
import Units 1.0
import QtQuick.Layouts 1.1

Item {

    property QtObject roomItemModel

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

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: parent.horizontalCenter
            rightMargin: Units.dp(160)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }
            clip: true

            ItemListView {
                id: listViewLeft
                model: roomItemModel

                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                showHeader: false
            }
            ScrollBar { listObject: listViewLeft }
        }
    }

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        clip: true

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            left: parent.horizontalCenter
            leftMargin: Units.dp(160)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }
            clip: true

            ItemListView {
                id: listViewRight
                model: roomItemModel

                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                showHeader: false
            }

            ScrollBar { listObject: listViewRight }
        }
    }

    Image {
        source: "qrc:/img/rooms/kitchen/kitchen_big.png"

        anchors.centerIn: parent
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
            text: qsTr("Room blabla")
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
