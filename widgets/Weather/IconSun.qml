import QtQuick 2.5
import SharedComponents 1.0

Item {

    property QtObject weatherModel

    anchors.fill: parent

    property bool isNight: weatherModel.isNight

    function getIconSun() {
        var date = new Date;
        if (date.getHours() < 11) {
            return "01"
        } else if (date.getHours() < 18) {
            return "02"
        }
        return "03"
    }

    Image {
        source: isNight?
                    "qrc:/widgets/Weather/moon.png":
                    "qrc:/widgets/Weather/sun%1.png".arg(getIconSun())

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-12)
        }
    }
}
