import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2
import Calaos 1.0
import "calaos.js" as Calaos;

Window {
    id: rootWindow
    visible: true
    width: 320 * calaosApp.density

    //iphone4
    height: 480 * calaosApp.density
    //iphone5
    //height: 568

    property bool isLandscape: rootWindow.width > rootWindow.height

    property variant roomModel
    property string currentRoomName

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Connections {
        target: calaosApp
        onApplicationStatusChanged: {
            if (calaosApp.applicationStatus === Common.LoggedIn) {
                menuBar.menuType = Common.MenuMain
                stackView.push(favoriteView)
            }
            else if (calaosApp.applicationStatus === Common.NotConnected)
                stackView.pop(loginView)
        }
    }

    function handleBack() {
        if (stackView.depth > 2) {
            stackView.pop()
            if (stackView.depth === 2)
                menuBar.menuType = Common.MenuMain
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
        Keys.onReleased: if (event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) {
                             handleBack()
                             event.accepted = true;
                         }
    }

    Component {
        id: favoriteView
        Item {
            Image {
                source: calaosApp.getPictureSized(isLandscape?
                                                      "background_landscape":
                                                      "background")
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }
            FavoritesListView {
                id: listViewFav
                //model: favoriteModel

                width: parent.width
                height: parent.height - menuBar.height
            }
        }
    }

    Component {
        id: homeView
        Item {
            Image {
                source: calaosApp.getPictureSized(isLandscape?
                                                      "background_landscape":
                                                      "background")
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }

            RoomListView {
                id: listViewRoom
                model: homeModel

                width: parent.width
                height: parent.height - menuBar.height

                onRoomClicked: {
                    //get room model
                    console.debug("model: " + homeModel)
                    roomModel = homeModel.getRoomModel(idx)
                    currentRoomName = room_name
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
                source: calaosApp.getPictureSized(isLandscape?
                                                      "background_landscape":
                                                      "background")
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }
            ItemListView {
                id: listViewItems
                model: roomModel

                roomName: currentRoomName

                width: parent.width
                height: parent.height - menuBar.height
            }
            ScrollBar {
                width: 10; height: listViewItems.height
                anchors.right: parent.right
                opacity: 1
                orientation: Qt.Vertical
                wantBackground: false
                position: listViewItems.visibleArea.yPosition
                pageSize: listViewItems.visibleArea.heightRatio
            }
        }
    }

    Component {
        id: mediaView
        Item {
            Image {
                source: calaosApp.getPictureSized(isLandscape?
                                                      "background_landscape":
                                                      "background")
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }

            ViewHeader {
                id: header
                headerLabel: qsTr("Media")
                iconSource: calaosApp.getPictureSized("icon_media")
            }

            Flow {
                id: listViewItems
                spacing: 10 * calaosApp.density

                MediaMenuItem {
                    label: qsTr("Music")
                    icon: IconMusic {}
                    onButtonClicked: stackView.push(musicView)
                }

                MediaMenuItem {
                    label: qsTr("Surveillance")
                    icon: IconCamera {}
                }

                width: Math.floor(parent.width / (198 * calaosApp.density)) * 198 * calaosApp.density
                height: parent.height - header.height - menuBar.height
                y: header.height

                anchors.centerIn: parent
                anchors.verticalCenterOffset: 30 * calaosApp.density
            }
        }
    }

    Component {
        id: musicView
        Item {
            Image {
                source: calaosApp.getPictureSized(isLandscape?
                                                      "background_landscape":
                                                      "background")
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
            }

            MusicListView {
                id: musicViewRoom
                model: audioModel

                width: parent.width
                height: parent.height - menuBar.height
            }
            ScrollBar {
                width: 10; height: musicViewRoom.height
                anchors.right: parent.right
                opacity: 1
                orientation: Qt.Vertical
                wantBackground: false
                position: musicViewRoom.visibleArea.yPosition
                pageSize: musicViewRoom.visibleArea.heightRatio
            }
        }
    }

    Loading {
        z: 9999 //on top of everything
        opacity: calaosApp.applicationStatus === Common.Loading?1:0
    }

    MainMenuBar {
        id: menuBar

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        onButtonHomeClicked: {
            menuBar.menuType = Common.MenuBack
            stackView.push(homeView)
        }
        onButtonMediaClicked: {
            menuBar.menuType = Common.MenuBack
            stackView.push(mediaView)
        }
        onButtonScenariosClicked: {
            menuBar.menuType = Common.MenuBack
            stackView.push(homeView)
        }
        onButtonConfigClicked: {
            menuBar.menuType = Common.MenuBack
            stackView.push(homeView)
        }
        onButtonBackClicked: {
            handleBack()
        }

        state: calaosApp.applicationStatus === Common.LoggedIn?"visible":"invisible"

        states: [
            State {
                name: "visible"
                PropertyChanges { target: menuBar; opacity: 1 }
                PropertyChanges { target: menuBar; anchors.bottomMargin: 0 }
            },
            State {
                name: "invisible"
                PropertyChanges { target: menuBar; opacity: 0.2 }
                PropertyChanges { target: menuBar; anchors.bottomMargin: -menuBar.height }
            }
        ]

        transitions: [
            Transition {
                from: "invisible"
                to: "visible"
                NumberAnimation { properties: "opacity,anchors.bottomMargin"; easing.type: Easing.OutExpo; duration: 500 }
            },
            Transition {
                from: "visible"
                to: "invisible"
                NumberAnimation { properties: "opacity,anchors.bottomMargin"; easing.type: Easing.InExpo; duration: 500 }
            }
        ]
    }
}
