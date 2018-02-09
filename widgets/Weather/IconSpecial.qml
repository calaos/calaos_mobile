import QtQuick 2.5
import SharedComponents 1.0

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
