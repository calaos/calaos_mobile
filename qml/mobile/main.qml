import QtQuick
import QtQuick.Window
import QtQuick.Controls
import Calaos
import SharedComponents
import "../quickflux"
import QuickFlux

Window {
    id: rootWindow
    visible: true

    //Units.dp() is the single sizing unit; it only knows the screen density
    //once we hand it over. Desktop's main.qml does the same.
    Component.onCompleted: {
        Units.cachedValue = Qt.binding(function() {
            return calaosApp.density;
        });
    }

    minimumWidth: Units.dp(320)
    minimumHeight: Units.dp(480)

    color: "#080808"

    //On iOS the window stops at the safe area unless it asks for the whole
    //screen; the plain black bands the system leaves above and below it look
    //letterboxed. Android 15+ always gives the whole screen (edge to edge);
    //older Android keeps the system bars outside the window, as before.
    flags: Qt.platform.os === "ios"? Qt.Window | Qt.ExpandedClientAreaHint : Qt.Window

    property bool isLandscape: rootWindow.width > rootWindow.height

    //Insets of the system UI: status bar, notch / Dynamic Island, home
    //indicator, and the Android system bars once the app is laid out edge to
    //edge (enforced from Android 15 with targetSdk 35+). SafeArea is in the
    //same units as the items and follows rotation and iPad window resizing;
    //it is 0 wherever the system keeps the window clear of its bars (Android
    //14 and older). Backgrounds run under the bars, text and controls do not.
    readonly property real safeTop: SafeArea.margins.top
    readonly property real safeBottom: SafeArea.margins.bottom
    readonly property real safeLeft: SafeArea.margins.left
    readonly property real safeRight: SafeArea.margins.right

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
            topMargin: rootWindow.safeTop
            bottomMargin: rootWindow.safeBottom
            leftMargin: rootWindow.safeLeft
            rightMargin: rootWindow.safeRight
        }
        fillMode: Image.PreserveAspectCrop
    }

    Connections {
        target: calaosApp
        function onApplicationStatusChanged(status) {
            //iOS background cycle with a page to keep (box updates): the
            //session reconnects in place, see ResumePolicy.
            if (calaosApp.keepPageOnResume)
                return
            if (calaosApp.applicationStatus === Common.LoggedIn) {
                menuBar.menuType = Common.MenuMain
                stackView.push(favoriteView)
            }
            else if (calaosApp.applicationStatus === Common.NotConnected)
                stackView.pop(null, StackView.Immediate)
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
            //While the menu bar is hidden (login page) the pages still stop
            //above the bottom inset
            bottomMargin: Math.max(0, rootWindow.safeBottom - (rootWindow.height - menuBar.y))
        }

        initialItem: loginView

        // Implements back key navigation. At the root (login page, or the
        // favourites once logged in) the key is left unaccepted: Qt then moves
        // the task to the background, as Android's own back does at the root
        // of an app. Open sheets take the key before it gets here, and a
        // visible system keyboard takes the first back to close itself.
        //
        // Android 13+ delivers a back that comes in as a key event (hardware
        // or keyboard back key, "adb shell input keyevent BACK") twice: as the
        // key itself and again through the OnBackInvokedCallback that Qt
        // registers, giving press, press, release, release. The gesture and
        // the navigation bar button only come through the callback. So only
        // presses seen here count (auto-repeat of a held key aside), and one
        // back is acted on when the last of them is released: the copy would
        // otherwise pop a second page, or send the app to the background
        // after the first one popped back to the root. A release with no
        // pending press (its press went to a sheet) is dropped.
        property int backPending: 0
        function resetBack() {
            backPending = 0
        }
        // A press whose release never comes here must not leave a count
        // behind that would eat the next back
        onActiveFocusChanged: if (!activeFocus) resetBack()
        Connections {
            target: Qt.application
            function onStateChanged() {
                stackView.resetBack()
            }
        }
        focus: true
        Keys.onPressed: (event) => {
                            if (event.key !== Qt.Key_Back)
                                return
                            if (!event.isAutoRepeat)
                                backPending++
                            // Taking the press as well when a page will be
                            // popped makes Qt's key-event path less likely to
                            // move the task to the background anyway (Qt 6.12
                            // race, see the qt6-android-build skill)
                            event.accepted = stackView.depth > 2
                        }
        Keys.onReleased: (event) => {
                             if (event.key !== Qt.Key_Back)
                                 return
                             if (backPending === 0) {
                                 event.accepted = true
                                 return
                             }
                             backPending--
                             if (backPending > 0) {
                                 event.accepted = true
                                 return
                             }
                             if (stackView.depth <= 2) {
                                 event.accepted = false
                                 return
                             }
                             rootWindow.handleBack()
                             event.accepted = true
                         }
    }

    Component {
        id: loginView

        //The fields fill themselves from calaosApp (LoginView.syncFromApp()).
        LoginView {
            onLoginClicked: calaosApp.login(username, password, hostname)
            onCancelClicked: calaosApp.logout()
        }
    }

    Component {
        id: favoriteView

        FavoritesListView {
        }
    }

    Component {
        id: homeView

        RoomListView {
            model: homeModel

            onRoomClicked: (idx, room_name) => {
                //get room model
                roomModel = homeModel.getRoomModel(idx)
                currentRoomName = room_name
                stackView.push(roomDetailView)
            }
        }
    }

    Component {
        id: roomDetailView

        RoomDetailView {
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
            roomItemModel: lightOnClonedModel
        }
    }

    Component {
        id: scenarioView

        ScenarioView {
        }
    }

    Component {
        id: mediaView

        MediaMenuView {
        }
    }

    Component {
        id: musicView

        MusicListView {
        }
    }

    Component {
        id: cameraView

        CameraListView {
        }
    }

    property variant currentCameraModel
    Component {
        id: cameraSingleView

        CameraSingleView {
            modelData: currentCameraModel
        }
    }

    Component {
        id: settingsView

        SettingsView {
            onFavoriteAddClicked: {
                menuBar.menuType = Common.MenuBack
                stackView.push(favAddView)
            }
            onFavoriteEditClicked: {
                menuBar.menuType = Common.MenuBack
                stackView.push(favEditView)
            }
            onBoxUpdatesClicked: {
                menuBar.menuType = Common.MenuBack
                stackView.push(updatesView)
            }
        }
    }

    Component {
        id: updatesView

        UpdatesView {
        }
    }

    Component {
        id: favAddView

        FavoritesAddView {
        }
    }

    Component {
        id: favEditView

        FavoritesEditView {
        }
    }

    Component {
        id: eventLogView

        EventLogView {
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

    property QtObject sensorDetailModel
    Component {
        id: sensorDetailView

        SensorDetailView {
            sensor: sensorDetailModel
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
            itemColor: itemRgbColor
            itemCallback: itemColorCallback
        }
    }

    //The bar runs from edge to edge and down to the bottom of the screen,
    //under the home indicator / navigation bar; its buttons stay inside.
    MainMenuBar {
        id: menuBar

        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        bottomInset: rootWindow.safeBottom
        leftInset: rootWindow.safeLeft
        rightInset: rootWindow.safeRight

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

    //No network while a session is wanted (FIX-10): at the top of the
    //content, inside the safe area. It only shows on the login page: losing
    //the network ends the session, which pops back to it.
    OfflineBanner {
        id: offlineBanner
        anchors {
            top: background.top
            left: background.left
            right: background.right
        }
    }

    Connections {
        target: calaosApp
        function onLoginWaitsForNetwork() {
            offlineBanner.highlight()
        }
    }

    //Those Rectangles mask the insets around the pages instead of clipping
    //them. The top one is the colour of the page headers, which therefore
    //appear to run up under the status bar. Below the pages, the menu bar
    //already covers the bottom inset.
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
            top: background.top
            bottom: menuBar.top
            left: parent.left; right: background.left
        }
    }
    Rectangle {
        color: "#080808"
        anchors {
            top: background.top
            bottom: menuBar.top
            left: background.right; right: parent.right
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

        Filter {
            type: ActionTypes.showClimateDetails
            onDispatched: (filtertype, message) => {
                climateSheet.openFor(message.io)
            }
        }

        Filter {
            type: ActionTypes.showSensorDetails
            onDispatched: (filtertype, message) => {
                sensorDetailModel = message.sensor
                stackView.push(sensorDetailView)
            }
        }

        //An io asks for a new value: on mobile we use the platform's native
        //input dialog. IOBase used to do this itself; it is a model, it has no
        //business knowing that dialogs exist (T31).
        Filter {
            type: ActionTypes.openAskTextForIo
            onDispatched: (filtertype, message) => {
                rootWindow.askTextIo = message.io
                hardwareUtils.inputTextDialog(qsTr("Change value"),
                                              qsTr("Enter new value"))
            }
        }
    }

    //The io waiting for the native input dialog to come back, null when no
    //dialog is pending. Guards against a stale answer being sent to the wrong
    //io, which the old C++ path did not do on cancel.
    property QtObject askTextIo: null

    Connections {
        target: hardwareUtils

        function onDialogTextValid(text) {
            if (!rootWindow.askTextIo)
                return
            var io = rootWindow.askTextIo
            rootWindow.askTextIo = null
            io.sendStringValue(text)
        }

        function onDialogCanceled() {
            rootWindow.askTextIo = null
        }
    }

    //Settings of a climate device, a bottom sheet over the current view
    ClimateSheet { id: climateSheet }

    //Unknown or changed server certificate: asked while logging in, over the
    //login page.
    TlsTrustSheet { id: tlsTrustSheet }

    //Admin code pushed to this phone, decrypted by the app: over any page,
    //logged in or not.
    AdminCodeSheet { id: adminCodeSheet }
}
