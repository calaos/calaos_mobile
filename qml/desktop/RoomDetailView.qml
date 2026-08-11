import QtQuick
import SharedComponents
import QtQuick.Layouts
import Calaos

PageScaffold {
    id: page

    property QtObject roomItemModel

    title: currentRoomName

    footerButtons: [
        FooterButton {
            label: qsTr("Back to home")
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
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
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
                model: RoomFilterModel {
                    source: roomItemModel
                    filter: Common.FilterLeft
                    scenarioVisible: false
                }

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
            top: page.header.bottom; topMargin: Units.dp(20)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
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
                model: RoomFilterModel {
                    source: roomItemModel
                    filter: Common.FilterRight
                    scenarioVisible: false
                }

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

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        clip: true

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            right: roomIcon.right; rightMargin: Units.dp(20)
            left: roomIcon.left; leftMargin: Units.dp(20)
            top: roomIcon.bottom; topMargin: Units.dp(40)
            bottom: page.footer.top; bottomMargin: Units.dp(20)
        }

        Item {
            anchors {
                fill: parent
                topMargin: Units.dp(2)
                bottomMargin: Units.dp(2)
            }
            clip: true

            ItemListView {
                id: listViewScenario
                model: RoomFilterModel {
                    source: roomItemModel
                    filter: Common.FilterScenario
                }

                anchors {
                    fill: parent
                    topMargin: Units.dp(3)
                    bottomMargin: Units.dp(3)
                    leftMargin: Units.dp(5)
                    rightMargin: Units.dp(5)
                }

                showHeader: false
            }

            ScrollBar { listObject: listViewScenario }
        }
    }

    Image {
        id: roomIcon
        source: "qrc:/img/rooms/%1/%1_big.png".arg(CalaosJs.getRoomTypeIcon(currentRoomType))

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: page.header.bottom; topMargin: Units.dp(30)
        }
    }
}
