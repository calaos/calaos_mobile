import QtQuick 2.0

Item {

    property string imageSource

    signal buttonClicked()

    width: 37 * calaosApp.density
    height: 31 * calaosApp.density

    Image {
        id: img

        source: calaosApp.getPictureSized(imageSource)

        anchors.fill: parent

        fillMode: Image.PreserveAspectFit

        MouseArea {
            anchors.fill: parent
            onClicked: buttonClicked()
        }
    }
}
