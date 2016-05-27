pragma Singleton
import QtQuick 2.5
import QuickFlux 1.0
import "./"

QtObject {

    // Homeboard link clicked
    function clickHomeboardItem(txt) {
        AppDispatcher.dispatch(ActionTypes.clickHomeboardItem, { text: txt });
    }

}
