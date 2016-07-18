import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import QuickFlux 1.0
import "../quickflux"

MouseArea {
    property variant camModel
    property bool camConnected: false

    width: Units.dp(218)
    height: Units.dp(300)

    opacity: camConnected?1:0.5

    onClicked: if (camConnected) AppActions.openCameraSingleView(camModel)

    Image {
        id: ico
        source: "qrc:/img/icon_camera_title.png"
        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            top: parent.top; topMargin: Units.dp(7)
        }
    }

    Text {
        id: title
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(13)
        font.family: calaosFont.fontFamily
        horizontalAlignment: Text.AlignLeft
        font.weight: Font.Light
        color: Theme.whiteColor
        anchors {
            left: ico.right; leftMargin: Units.dp(5)
            verticalCenter: ico.verticalCenter
            right: parent.right; rightMargin: Units.dp(5)
        }
        text: camConnected?camModel.name:"No camera"
    }

    Image {
        id: cambg
        source: "qrc:/img/camera_border.png"
        anchors {
            horizontalCenter: parent.horizontalCenter
            top: ico.bottom; topMargin: Units.dp(4)
        }

        Image {
            source: "qrc:/img/camera_nocam.png"
            anchors.centerIn: parent
        }

        Image {
            id: cameraPic
            source: camConnected?camModel.url_single:""
            anchors {
                fill: parent
                leftMargin: Units.dp(19); rightMargin: Units.dp(19)
                topMargin: Units.dp(18); bottomMargin: Units.dp(18)
            }
        }
    }

    Image {
        source: "qrc:/img/camera_reflect.png"
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom; bottomMargin: Units.dp(-11)
        }
    }

}
