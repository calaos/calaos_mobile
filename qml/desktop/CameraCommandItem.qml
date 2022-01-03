import QtQuick
import SharedComponents
import QtQuick.Layouts
import Calaos
import "."

ColumnLayout {
    spacing: Units.dp(2)

    Text {
        id: moveZoom
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Zoom")
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        clip: true
        elide: Text.ElideMiddle
    }

    RowLayout {
        spacing: Units.dp(32)
        Layout.alignment: Qt.AlignHCenter

        ItemButtonAction {
            id: bPTZ_zoomout
            Layout.alignment: Qt.AlignHCenter
            imageSource: "button_min"
            onButtonClicked: camModel.cameraZoomOut()
        }

        ItemButtonAction {
            id: bPTZ_zoomIn
            Layout.alignment: Qt.AlignHCenter
            imageSource: "button_plus"
            onButtonClicked: camModel.cameraZoomIn()
        }
    }

    Item {
        height: Units.dp(32)
    }

    Text {
        id: moveText
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Déplacement")
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        clip: true
        elide: Text.ElideMiddle
    }

    ItemButtonAction {
        id: bPTZ_up
        Layout.alignment: Qt.AlignHCenter
        imageSource: "button_up"
        onButtonClicked: camModel.cameraMoveUp()
    }

    RowLayout {
        spacing: 2
        Layout.alignment: Qt.AlignHCenter

        ItemButtonAction {
            id: bPTZ_left
            imageSource: "button_right"
            rotation: 180
            onButtonClicked: camModel.cameraMoveLeft()
        }
        ItemButtonAction {
            id: bPTZ_stop
            imageSource: "button_stop"
            onButtonClicked: camModel.cameraMoveStop()
        }
        ItemButtonAction {
            id: bPTZ_right
            imageSource: "button_right"
            onButtonClicked: camModel.cameraMoveRight()
        }
    }

    ItemButtonAction {
        id: bPTZ_down
        imageSource: "button_down"
        Layout.alignment: Qt.AlignHCenter
        onButtonClicked: camModel.cameraMoveDown()
    }

}
