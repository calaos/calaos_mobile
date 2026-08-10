import QtQuick
import QtQuick.Controls
import Calaos
import SharedComponents
import QuickFlux
import "../quickflux"
import QtQuick.VirtualKeyboard.Settings

Window {
    id: rootWindow
    visible: true

    width: Units.dp(1024) * calaosApp.density
    height: Units.dp(768) * calaosApp.density

    property QtObject roomModel
    property string currentRoomName
    property string currentRoomType
    property QtObject cameraSingleModel

    property bool isSingleCameraView: false

    //this is called by HardwareUtils
    function showAlertMessage(title, message, buttonText, timeout) {
        notif.showMessage(title, message, timeout)
    }

    //this is called by HardwareUtils
    function showNetworkActivity(en) {
        //TODO
    }

    function goToDesktop() {
        mainMenu.unselectAll()
        stackView.pop(null)
    }

    function handleBack() {
        //enable all cameras if going back to CameraListView
        if (isSingleCameraView) {
            cameraModel.cameraVisible = true
            isSingleCameraView = false
        }
        else
            cameraModel.cameraVisible = false

        if (stackView.depth > 1) {
            stackView.pop()
            if (stackView.depth == 1)
                mainMenu.unselectAll()
        }
    }

    //viewId of a page on the navigation stack, "" when it declares none
    function viewIdOf(page) {
        return (page && page.viewId) ? page.viewId : ""
    }

    //Show a top level section, replacing whatever section was open. The desktop
    //is the stack root and stays there, so a section always ends up at depth 2.
    function enterSection(sectionView) {
        stackView.pop(null)
        stackView.push(sectionView)
    }

    //Open a Configuration subpage from anywhere, the way a user would reach it:
    //Configuration in the main menu, then the tile. handleSubitemClick() alone
    //only pushes the page, it assumes the section was already entered, so
    //calling it from outside leaves currentButton at 0. The footer selection
    //and the desktop chrome both follow currentButton, and "Back to config"
    //needs the config panel underneath.
    function goToConfigSubitem(itemId) {
        if (viewIdOf(stackView.currentItem) === itemId)
            return

        if (mainMenu.currentButton !== 4) {
            enterSection(configPanelView)
            mainMenu.currentButton = 4
        }
        else {
            //already in the section but on a deeper page: entering a section
            //always leaves its root at depth 2, so pop back down to it
            while (stackView.depth > 2)
                stackView.pop()
        }

        handleSubitemClick(itemId)
    }

    function handleSubitemClick(itemId) {
        var item;
        if (itemId === "media/music") {
            item = musicListView
        }
        else if (itemId === "media/camera") {
            item = cameraListView
        } else if (itemId === "media/web") {
            item = webView
        } else if (itemId === "config/screen") {
            item = configScreen
        } else if (itemId === "config/l18n") {
            item = configL18nView
        } else if (itemId === "config/info") {
            item = configUserInfoView
        } else if (itemId === "media/spotify") {
            item = spotifyView
        } else if (itemId === "config/network") {
            item = configNetworkView
        } else if (itemId === "config/update") {
            item = configUpdateView
        } else if (itemId === "config/install") {
            item = configInstallView
        } else if (itemId === "config/rollback") {
            dialogRecoveryBoot.show()
            return
        }

        stackView.push(item)
    }

    function openColorPicker(item, cb) {
        dialogRgbColorPicker.openWithIO(item, cb)
    }

    //Load fonts
    Fonts { id: calaosFont }

    Background {
        anchors.fill: parent

        menuContent: MainMenu {
            id: mainMenu

            //A section always sits at depth 2, directly above the desktop.
            //replace(desktopView, x) used to be called when coming from another
            //section, but desktopView is a Component and replace() only treats
            //its first argument as a target when that target is an Item already
            //on the stack. It was instead replacing the current page with two
            //pages, growing the stack and leaving a stray desktop underneath.
            onButtonHomeClicked: enterSection(homeView)
            onButtonMediaClicked: enterSection(mediaMenuView)
            onButtonScenariosClicked: enterSection(scenariosView)
            onButtonConfigClicked: enterSection(configPanelView)
        }

        mainContent: StackViewAnim {
            id: stackView
            anchors.fill: parent

            initialItem: desktopView

            // Implements back key navigation
            focus: true
            Keys.onReleased: (event) => {
                                 if (event.key === Qt.Key_Back || event.key === Qt.Key_Backspace) {
                                     handleBack()
                                     event.accepted = true;
                                 }
                             }

            onCurrentItemChanged: {
                if ('hideMainMenu' in currentItem) {
                    if (currentItem.hideMainMenu)
                        AppActions.hideMainMenu()
                    else
                        AppActions.showMainMenu()
                } else
                    AppActions.showMainMenu()
            }
        }
    }

    Component.onCompleted: {
        Units.cachedValue = Qt.binding(function() {
            return calaosApp.density;
        });

        VirtualKeyboardSettings.styleName = "calaos"

        if (calaosApp.isSnapshotBoot) {

        }
    }

    Component {
        id: desktopView

        DesktopView {
        }
    }

    Component {
        id: homeView

        HomeView {
            model: homeModel

            onRoomClicked: (idx, room_name, room_type) => {
                //get room model
                console.debug("model: " + homeModel)
                roomModel = homeModel.getRoomModel(idx)
                currentRoomName = room_name
                currentRoomType = room_type
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

    Component {
        id: cameraSingleView

        CameraSingleView {
            height: parent.height
            width: parent.width

            camModel: cameraSingleModel
        }
    }

    Component {
        id: mediaMenuView
        MediaView {}
    }

    Component {
        id: scenariosView
        ScenarioView {}
    }

    Component {
        id: configPanelView
        ConfigView {}
    }

    Component {
        id: musicListView
        MusicListView {}
    }

    Component {
        id: cameraListView
        CameraListView {}
    }

    Item {
        //Webview are not deleted when popped from StackView.
        //It allows user to keep the current website open to it's last page
        id: webParent
        visible: false

        Loader {
            id: webView
            property bool hideMainMenu: true
            source: "qrc:/qml/desktop/MediaWebView.qml"
        }
        Loader {
            id: spotifyView
            property bool hideMainMenu: true
            source: "qrc:/qml/desktop/SpotifyView.qml"
        }
    }

    Component {
        id: configScreen
        ConfigScreenView {}
    }

    Component {
        id: configL18nView
        ConfigL18nView {}
    }

    Component {
        id: configUserInfoView
        ConfigUserInfoView {}
    }

    Component {
        id: configNetworkView
        ConfigNetworkView {}
    }

    Component {
        id: configUpdateView
        ConfigUpdateView {}
    }

    Component {
        id: configInstallView
        ConfigInstallView {}
    }

    Notification {
        id: notif
        anchors {
            top: parent.top
            horizontalCenter: parent.horizontalCenter
        }
    }

    //Non modal update notice. Declared before the overlay layer so that a
    //dialog, which reparents itself into that layer, still covers it.
    UpdateToast {
        id: updateToast

        anchors {
            top: parent.top; topMargin: Units.dp(32)
            right: parent.right; rightMargin: Units.dp(16)
        }

        onAccepted: rootWindow.goToConfigSubitem("config/update")
    }

    //This overlay is for displaying dialogs on top of everything
    OverlayLayer {
        id: dialogOverlayLayer
        objectName: "dialogOverlayLayer"
    }

    DialogReboot { id: dialogReboot }

    DialogRGBColorPicker { id: dialogRgbColorPicker }

    DialogKeyboard { id: dialogKeyboard }

    DialogRecoveryBoot { id: dialogRecoveryBoot }

    DialogSensorDetails { id: dialogSensorDetails }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.clickHomeboardItem
            onDispatched: (filtertype, message) => {
                if (message.text === "reboot") {
                    dialogReboot.showDialog(true, true)
                } else if (message.text === "screensaver") {
                    AppActions.suspendScreen()
                }
            }
        }

        Filter {
            type: ActionTypes.showRebootDialog
            onDispatched: (filtertype, message) => {
                dialogReboot.showDialog(message.showMachine, message.showApp)
            }
        }

        Filter {
            type: ActionTypes.openCameraSingleView
            onDispatched: (filtertype, message) => {
                cameraSingleModel = message.camModel
                stackView.push(cameraSingleView)
            }
        }
        Filter {
            type: ActionTypes.openAskTextForIo

            property QtObject io

            onDispatched: (filtertype, message) => {
                io = message.io
                console.log("todo keyboard for item:" + io + " - " + io.ioName)
                dialogKeyboard.openKeyboard(qsTr("Keyboard"),
                                            qsTr("Change text for '%1'").arg(io.ioName),
                                            io.stateString,
                                            TextInput.Normal,
                                            false,
                                            function (txt) {
                                                io.sendStringValue(txt)
                                            })
            }
        }
        Filter {
            type: ActionTypes.openKeyboard

            onDispatched: (filtertype, message) => {
                //check if message.returnAction is of type function
                let retFunc
                if (typeof message.returnAction === "function") {
                    retFunc = message.returnAction
                } else {
                    retFunc = function (txt) {
                        AppDispatcher.dispatch(message.returnAction,
                                               { text: txt,
                                                 returnPayload: message.returnPayload
                        })
                    }
                }

                dialogKeyboard.openKeyboard(message.title,
                                            message.subtitle,
                                            message.initialText,
                                            message.inputEchoMode,
                                            message.multiline,
                                            retFunc
                                            )
            }
        }

        Filter {
            type: ActionTypes.showNotificationMsg

            onDispatched: (filtertype, message) => {
                if (message.hasOwnProperty("timeout")) {
                    rootWindow.showAlertMessage(message.title, message.message, message.button, message.timeout)
                } else {
                    rootWindow.showAlertMessage(message.title, message.message, message.button)
                }
            }
        }

        Filter {
            type: ActionTypes.showReadOnlyBootDialog

            onDispatched: (filtertype, message) => {
                dialogRecoveryBoot.show()
            }
        }

        Filter {
            type: ActionTypes.showSensorDetails

            onDispatched: (filtertype, message) => {
                dialogSensorDetails.showSensor(message.sensor)
            }
        }

        Filter {
            type: ActionTypes.showUpdateAvailableDialog

            onDispatched: (filtertype, message) => {
                //never surface the notice while an upgrade is already running
                if (updateManager.isUpgrading)
                    return
                updateToast.showToast(message)
            }
        }
    }

    //This should stay at the top of all object layer
    ScreenSuspend {
        id: screenSuspend
    }
}
