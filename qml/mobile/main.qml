import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2
import Calaos 1.0
import SharedComponents 1.0
import "../quickflux"
import QuickFlux 1.0

Window {
    id: rootWindow
    visible: true

    minimumWidth: 320 * calaosApp.density
    minimumHeight: 480 * calaosApp.density

    color: "#080808"

    //flags: Qt.platform.os === "ios"? Qt.MaximizeUsingFullscreenGeometryHint : undefined

    property bool isLandscape: rootWindow.width > rootWindow.height

    property variant roomModel
    property string currentRoomName

    property bool isSingleCameraView: false

    //Load fonts
    Fonts { id: calaosFont }

    Image {
        id: background
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors {
            fill: parent
            topMargin: platformMarginsTop
            bottomMargin: platformMarginsBottom
            leftMargin: platformMarginsLeft
            rightMargin: platformMarginsRight
        }
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
        //enable all cameras if going back to CameraListView
        if (isSingleCameraView) {
            cameraModel.cameraVisible = true
            isSingleCameraView = false
        }
        else
            cameraModel.cameraVisible = false

        if (stackView.depth > 2) {
            stackView.pop()
            if (stackView.depth === 2)
                menuBar.menuType = Common.MenuMain
        }
    }

    StackView {
        id: stackView
        anchors {
            top: background.top
            left: background.left
            right: background.right
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
            height: background.height
            width: background.width
        }
    }

    Component {
        id: homeView

        RoomListView {
            height: background.height
            width: background.width

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
            height: background.height
            width: background.width

            filteredRoomItemModel: roomModel
        }
    }

    function openLightsOnView() {
        menuBar.menuType = Common.MenuBack
        lightOnClonedModel = lightOnModel.getQmlCloneModel()
        stackView.push(lightsOnDetailView)
    }

    property QtObject lightOnClonedModel
    Component {
        id: lightsOnDetailView

        RoomDetailView {
            id: lightsOnRoomDetailView
            height: background.height
            width: background.width

            roomItemModel: lightOnClonedModel
        }
    }

    Component {
        id: scenarioView

        ScenarioView {
            width: background.width
            height: background.height
        }
    }

    Component {
        id: mediaView

        MediaMenuView {
            width: background.width
            height: background.height
        }
    }

    Component {
        id: musicView

        MusicListView {
            width: background.width
            height: background.height
        }
    }

    Component {
        id: cameraView

        CameraListView {
            width: background.width
            height: background.height
        }
    }

    property variant currentCameraModel
    Component {
        id: cameraSingleView

        CameraSingleView {
            modelData: currentCameraModel
            width: background.width
            height: background.height
        }
    }

    Component {
        id: settingsView

        SettingsView {
            width: background.width
            height: background.height - menuBar.height

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
            width: background.width
            height: background.height - menuBar.height
        }
    }

    Component {
        id: favEditView

        FavoritesEditView {
            width: background.width
            height: background.height - menuBar.height
        }
    }

    Component {
        id: eventLogView

        EventLogView {
            width: background.width
            height: background.height - menuBar.height
        }
    }

    property string pushEventText
    property string pushEventPicUrl
    property string pushEventUuid
    Component {
        id: pushEventView

        PushEventView {
            event_message: pushEventText
            event_picurl: pushEventPicUrl
            event_uuid: pushEventUuid
        }
    }

    function openColorPicker(item, cb) {
        menuBar.menuType = Common.MenuBack
        itemColorCallback = function(c) {
            handleBack()
            cb(c)
        }
        itemRgbColor = item
        stackView.push(colorPickerView)
    }

    property QtObject itemRgbColor
    property var itemColorCallback
    Component {
        id: colorPickerView

        ColorPickerRGBView {
            width: background.width
            height: background.height - menuBar.height
            itemColor: itemRgbColor
            itemCallback: itemColorCallback
        }
    }

    MainMenuBar {
        id: menuBar

        anchors {
            left: background.left
            right: background.right
            bottom: background.bottom
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
                PropertyChanges { target: menuBar; anchors.bottomMargin: -(menuBar.height + platformMarginsBottom) }
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

    //Those Rectangles are for masking top and bottom instead of using clipping
    Rectangle {
        color: "#080808"
        anchors {
            top: parent.top
            bottom: background.top
            left: parent.left; right: parent.right
        }
    }
    Rectangle {
        color: "#080808"
        anchors {
            top: menuBar.bottom
            bottom: parent.bottom
            left: parent.left; right: parent.right
        }
    }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.openEventLog
            onDispatched: (filtertype, message) => {
                stackView.push(eventLogView)
            }
        }

        Filter {
            type: ActionTypes.openEventPushViewer
            onDispatched: (filtertype, message) => {
                pushEventText = message.notifText
                pushEventPicUrl = message.notifUrl
                pushEventUuid = ""
                stackView.push(pushEventView)
            }
        }

        Filter {
            type: ActionTypes.openEventPushViewerUuid
            onDispatched: (filtertype, message) => {
                pushEventText = ""
                pushEventPicUrl = ""
                pushEventUuid = message.notifUuid
                stackView.push(pushEventView)
                menuBar.menuType = Common.MenuBack
            }
        }
    }
}
