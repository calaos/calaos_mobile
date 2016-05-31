import QtQuick 2.5
import SharedComponents 1.0

Item {

    property QtObject weatherModel

    anchors.fill: parent

    property bool isNight: weatherModel.isNight
    property int code: weatherModel.weatherCode

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
        source: weatherModel.isNight?
                    "qrc:/widgets/Weather/moon.png":
                    "qrc:/widgets/Weather/sun%1.png".arg(getIconSun())

        visible: code <= 802

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-14)
            horizontalCenterOffset: Units.dp(16)
        }
    }

    function getIconCloud() {
        if (code == 801) {
            return "01"
        } else if (code == 802) {
            return "02"
        }
        return "02"
    }

    Image {
        source: "qrc:/widgets/Weather/cloud%1.png".arg(getIconCloud())

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: code == 803?Units.dp(-14):Units.dp(-4)
            horizontalCenterOffset: code == 803?0:Units.dp(-7)
        }
    }

    Image {
        source: "qrc:/widgets/Weather/cloud03.png"

        visible: code == 804

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-12)
            horizontalCenterOffset: Units.dp(14)
        }
    }
}
