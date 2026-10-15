import QtQuick
import SharedComponents

Item {

    property QtObject weatherModel

    anchors.fill: parent

    Image {
        source: "qrc:/widgets/Weather/fog.png"

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-12)
        }
    }
}
