import QtQuick 2.5
import SharedComponents 1.0

Item {

    property QtObject weatherModel

    anchors.fill: parent

    property int code: weatherModel.weatherCode

    function getIconRain() {
        if (code == 500 ||
            code == 501 ||
            code == 520) {
            return "01"
        } else if (code == 502 ||
                   code == 521) {
            return "02"
        }
        return "03"
    }

    Image {
        source: "qrc:/widgets/Weather/rain%1.png".arg(getIconRain())

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-14)
        }
    }
}
