import QtQuick 2.2
import QtQuick.Window 2.1
import QtQuick.Controls 1.2
import Calaos 1.0
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

    Image {
        source: isLandscape?
                    "qrc:/img/background_landscape.png":
                    "qrc:/img/background.png"
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }
/*
    StackView {
        id: mainStack
        anchors.fill: parent
//        delegate: StackViewDelegate {
//            function transitionFinished(properties)
//            {
//                properties.exitItem.opacity = 1
//            }

//            pushTransition: StackViewTransition {
//                PropertyAnimation {
//                    target: enterItem
//                    property: "opacity"
//                    from: 0
//                    to: 1
//                }
//                PropertyAnimation {
//                    target: exitItem
//                    property: "opacity"
//                    from: 1
//                    to: 0
//                }
//            }
//        }
    }

    Component {
        id: homeView
        Item {
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
        }
    }

    Component {
        id: roomDetailView
        Item {
            ItemListView {
                id: listViewItems
                model: modelRoom
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
*/

    RoomListView {
        id: listViewRoom
        model: homeModel
    }
    ScrollBar {
        width: 10 * calaosApp.density; height: listViewRoom.height
        anchors.right: parent.right
        opacity: 1
        orientation: Qt.Vertical
        wantBackground: false
        position: listViewRoom.visibleArea.yPosition
        pageSize: listViewRoom.visibleArea.heightRatio
    }

    LoginView {
        opacity: calaosApp.applicationStatus == Common.NotConnected?1:0

        onLoginClicked: {
            calaosApp.login(username, password, hostname)
        }
    }

    Loading {
        opacity: calaosApp.applicationStatus == Common.Loading?1:0
    }
}
