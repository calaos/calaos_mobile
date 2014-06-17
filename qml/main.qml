import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2
import Calaos 1.0
import "calaos.js" as Calaos;

Window {
    id: rootWindow
    visible: true
    width: 320

    //iphone4
    height: 480
    //iphone5
    //height: 568

    property bool isLandscape: rootWindow.width > rootWindow.height

    property variant roomModel

    Image {
        source: isLandscape?
                    "qrc:/img/background_landscape.png":
                    "qrc:/img/background.png"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Connections {
        target: calaosApp
        onApplicationStatusChanged: {
            if (calaosApp.applicationStatus === Common.LoggedIn)
                stackView.push(homeView)
            else if (calaosApp.applicationStatus === Common.NotConnected)
                stackView.pop(loginView)
        }
    }

    StackView {
        id: stackView
        anchors.fill: parent

        initialItem: LoginView {
            onLoginClicked: calaosApp.login(username, password, hostname)
        }

        // Implements back key navigation
        focus: true
        Keys.onReleased: if ((event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) && stackView.depth > 2) {
                             stackView.pop();
                             event.accepted = true;
                         }
    }

    Component {
        id: homeView
        Item {
            Image {
                source: isLandscape?
                            "qrc:/img/background_landscape.png":
                            "qrc:/img/background.png"
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }
            RoomListView {
                id: listViewRoom
                model: homeModel

                onRoomClicked: {
                    //get room model
                    console.debug("model: " + homeModel)
                    roomModel = homeModel.getRoomModel(idx)
                    stackView.push(roomDetailView)
                }
            }
            ScrollBar {
                width: 10; height: listViewRoom.height
                anchors.right: parent.right
                opacity: 1
                orientation: Qt.Vertical
                wantBackground: false
                position: listViewRoom.visibleArea.yPosition
                pageSize: listViewRoom.visibleArea.heightRatio
            }
        }
    }

    Component {
        id: roomDetailView
        Item {
            Image {
                source: isLandscape?
                            "qrc:/img/background_landscape.png":
                            "qrc:/img/background.png"
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }
            ItemListView {
                id: listViewItems
                model: roomModel
            }
            ScrollBar {
                width: 10; height: listViewRoom.height
                anchors.right: parent.right
                opacity: 1
                orientation: Qt.Vertical
                wantBackground: false
                position: listViewRoom.visibleArea.yPosition
                pageSize: listViewRoom.visibleArea.heightRatio
            }
        }
    }

    Loading {
        z: 9999 //on top of everything
        opacity: calaosApp.applicationStatus == Common.Loading?1:0
    }
}
