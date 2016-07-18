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
}
