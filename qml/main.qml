import QtQuick 2.2
import QtQuick.Window 2.1
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
    property bool isLoading: false
    property bool isLoggedIn: false

    property var calaosObj

    //Global models
    ListModel
    {
        id: modelHome;

        function load(user, pass, host) {
            Calaos.loadHome(user, pass, host,
                            function(data) {
                                calaosObj = data;
                                isLoggedIn = true;
                                isLoading = false;

                                modelHome.clear()
                                if (calaosObj.home === undefined) return;

                                //sort rooms
                                calaosObj.home.sort(function (rooma, roomb) { return roomb.hits - rooma.hits; });

                                for (var it = 0; it < calaosObj.home.length; it++) {
                                    modelHome.append(calaosObj.home[it])
                                }
                            },
                            function() {
                                isLoggedIn = false;
                                isLoading = false;
                            });
            isLoading = true;
        }
    }

    Image {
        source: isLandscape?
                    "qrc:/img/background_landscape.png":
                    "qrc:/img/background.png"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    RoomListView {
        id: listViewRoom
        model: modelHome
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

    LoginView {
        opacity: !isLoading && !isLoggedIn?1:0

        onLoginClicked: {
            modelHome.load(username, password, hostname)
        }
    }

    Loading {
        opacity: isLoading?1:0
    }
}
