import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2
import Calaos 1.0
import "calaos.js" as Calaos;
import Units 1.0

Window {
    id: rootWindow
    visible: true

    width: Units.dp(1024) * calaosApp.density
    height: Units.dp(768) * calaosApp.density

    property variant roomModel
    property string currentRoomName

    property bool isSingleCameraView: false

    //Load fonts
    Fonts { id: calaosFont }

    BackgroundDesktop {
        anchors.fill: parent

        menuContent: MainMenu { }
    }

    Component.onCompleted: {
        Units.cachedValue = Qt.binding(function() {
            return calaosApp.density;
        });
    }
}
