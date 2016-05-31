import QtQuick 2.5
import SharedComponents 1.0

Item {

    property QtObject weatherModel

    anchors.fill: parent

    property bool isRain: weatherModel.weatherCode == 200 ||
                          weatherModel.weatherCode == 201 ||
                          weatherModel.weatherCode == 202 ||
                          weatherModel.weatherCode == 230 ||
                          weatherModel.weatherCode == 231 ||
                          weatherModel.weatherCode == 232

    Image {
        source: isRain?
                    "qrc:/widgets/Weather/thunderstorm02.png":
                    "qrc:/widgets/Weather/thunderstorm01.png"

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-12)
        }
    }
}
