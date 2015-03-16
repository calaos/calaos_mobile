import QtQuick 2.3

Item {

    signal buttonClicked()

    width: ico.implicitWidth
    height: ico.implicitHeight

    Rectangle {
        color: "#3ab4d7"
        anchors.centerIn: parent
        width: calaosApp.dp(47)
        height: calaosApp.dp(53)
    }

    Image {
        id: ico
        source: calaosApp.getPictureSized("button_voice")
        anchors.centerIn: parent
    }

    MouseArea {
        anchors.fill: parent
        onClicked: buttonClicked()
    }
}

