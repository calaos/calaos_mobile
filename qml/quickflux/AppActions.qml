pragma Singleton
import QtQuick 2.5
import QuickFlux 1.0
import "./"

QtObject {

    // Homeboard link clicked
    function clickHomeboardItem(txt) {
        AppDispatcher.dispatch(ActionTypes.clickHomeboardItem, { text: txt });
    }

    // Homeboard menu needs to be hidden (for example when editing widgets)
    function hideHomeboardMenu() {
        AppDispatcher.dispatch(ActionTypes.hideHomeboardMenu);
    }

    // Homeboard menu can be shown again
    function showHomeboardMenu() {
        AppDispatcher.dispatch(ActionTypes.showHomeboardMenu);
    }

    // Widgets save position
    function saveWidgetsPosition() {
        AppDispatcher.dispatch(ActionTypes.saveWidgetsPosition);
    }

    // Widgets reset position (cancel)
    function resetWidgetsPosition() {
        AppDispatcher.dispatch(ActionTypes.resetWidgetsPosition);
    }

    function wakeupScreen() {
        AppDispatcher.dispatch(ActionTypes.wakeupScreen);
    }

    function suspendScreen() {
        AppDispatcher.dispatch(ActionTypes.suspendScreen);
    }

    function openCameraSingleView(model) {
        AppDispatcher.dispatch(ActionTypes.openCameraSingleView, { camModel: model });
    }
	
	function openEventLog() {
		AppDispatcher.dispatch(ActionTypes.openEventLog);
	}

    function openEventPushViewer(text, picurl) {
        AppDispatcher.dispatch(ActionTypes.openEventPushViewer,
                               { notifText: text,
                                 notifUrl: picurl });
    }

    function openEventPushViewerUuid(uuid) {
        AppDispatcher.dispatch(ActionTypes.openEventPushViewer,
                               { notifUuid: uuid });
    }

    function openKeyboard(kTitle, kSubtitle, txt, kMultiline, kReturnAction, kReturnPayload) {
        AppDispatcher.dispatch(ActionTypes.openKeyboard,
                               { title: kTitle,
                                 subtitle: kSubtitle,
                                 initialText: txt,
                                 multiline: kMultiline,
                                 returnAction: kReturnAction,
                                 returnPayload: kReturnPayload });
    }

    function showMainMenu() {
        AppDispatcher.dispatch(ActionTypes.showMainMenu)
    }

    function hideMainMenu() {
        AppDispatcher.dispatch(ActionTypes.hideMainMenu)
    }

    function showNotificationMsg(nTitle, nMsg, nButton, nTimeout) {
        var m = { title: nTitle,
                  message: nMsg,
                  button: nButton };
        if (typeof nTimeout !== "undefined") {
            m.timeout = nTimeout
        }

        AppDispatcher.dispatch(ActionTypes.showNotificationMsg, m);
    }

    function newLogItem(txt, ccolor) {
        AppDispatcher.dispatch(ActionTypes.newLogItem, {
                                   line: txt,
                                   color: ccolor,
                               })
    }

    function showRebootDialog(showRebootMachine, showRebootApp) {
        AppDispatcher.dispatch(ActionTypes.showRebootDialog, {
                                showMachine: showRebootMachine,
                                showApp: showRebootApp,
                               });
    }
}
