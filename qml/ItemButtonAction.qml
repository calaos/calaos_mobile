import QtQuick 2.0

Item {

    property alias imageSource: img.source
    property alias sourceWidth: img.sourceSize.width
    property alias sourceHeight: img.sourceSize.height

    signal buttonClicked()

    width: img.width
    height: img.height

    Image {
        id: img

        MouseArea {
            anchors.fill: parent
            onClicked: buttonClicked()
        }
    }
}
