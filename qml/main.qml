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
                stackView.pop({ item: loginView, immediate: true })
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
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: menuBar.top
        }

        initialItem: loginView

        // Implements back key navigation
        focus: true
        Keys.onReleased: if (event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) {
                             handleBack()
                             event.accepted = true;
                         }
    }

    Component {
        id: loginView

        LoginView {
            username: calaosApp.username
            password: calaosApp.password
            hostname: calaosApp.hostname

            onLoginClicked: calaosApp.login(username, password, hostname)
        }
    }

    Component {
        id: favoriteView

        FavoritesListView {
            height: parent.height
            width: parent.width
        }
    }

    Component {
        id: homeView

        RoomListView {
            height: parent.height
            width: parent.width

            model: homeModel

            onRoomClicked: {
                //get room model
                console.debug("model: " + homeModel)
                roomModel = homeModel.getRoomModel(idx)
                currentRoomName = room_name
                stackView.push(roomDetailView)
            }
        }
    }

    Component {
        id: roomDetailView

        RoomDetailView {
            height: parent.height
            width: parent.width

            roomItemModel: roomModel
        }
    }

    function openLightsOnView() {
        menuBar.menuType = Common.MenuBack
        stackView.push(lightsOnDetailView)
    }

    Component {
        id: lightsOnDetailView

        RoomDetailView {
            height: parent.height
            width: parent.width

            roomItemModel: lightOnModel
        }
    }

    Component {
        id: scenarioView

        ScenarioView {
            width: parent.width
            height: parent.height
        }
    }

    Component {
        id: mediaView

        MediaMenuView {
            width: parent.width
            height: parent.height
        }
    }

    Component {
        id: musicView

        MusicListView {
            width: parent.width
            height: parent.height
        }
    }

    Component {
        id: cameraView

        CameraListView {
            width: parent.width
            height: parent.height
        }
    }

    Component {
        id: settingsView

        SettingsView {
            width: parent.width
            height: parent.height - menuBar.height

            onFavoriteAddClicked: {
                menuBar.menuType = Common.MenuBack
                stackView.push(favAddView)
            }
            onFavoriteEditClicked: {
                menuBar.menuType = Common.MenuBack
                stackView.push(favEditView)
            }
        }
    }

    Component {
        id: favAddView

        FavoritesAddView {
            width: parent.width
            height: parent.height - menuBar.height
        }
    }

    Component {
        id: favEditView

        FavoritesEditView {
            width: parent.width
            height: parent.height - menuBar.height
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
            stackView.push(scenarioView)
        }
        onButtonConfigClicked: {
            menuBar.menuType = Common.MenuBack
            stackView.push(settingsView)
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
