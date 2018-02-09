import QtQuick 2.5
import SharedComponents 1.0
import WeatherInfo 1.0

Item {
    id: widget

    property variant widgetModel
    property size minimumSize: bg.sourceSize
    property size maximumSize: bg.sourceSize

    WeatherModel {
        id: weatherModel
    }

    width: bg.width
    height: bg.height

    Fonts { id: calaosFont }

    Timer {
        //update weather data every hour
        interval: 3600000; running: true; repeat: true;
        onTriggered: weatherModel.refreshWeather()
        triggeredOnStart: true
    }

    Image {
        id: bg
        source: "widget_base_back.png"
    }

    Loader {
        property int code: weatherModel.weather.weatherCode

        function getIcon(c) {
            if (c >= 200 && c <= 232) {
                return "IconThunder.qml"
            } else if (c >= 300 && c <= 321 ||
                       c >= 500 && c <= 531) {
                return "IconRain.qml"
            } else if (c >= 600 && c <= 622) {
                return "IconSnow.qml"
            } else if (c >= 701 && c <= 781 ||
                       c >= 900) {
                return "IconSpecial.qml"
            } else if (c >= 801 && c <= 804) {
                return "IconCloud.qml"
            } else if (c == 800) {
                return "IconSun.qml"
            }

            console.log("Unknown weather icon code: " + c)
            return ""
        }

        source: getIcon(code)
        anchors.fill: parent

        onLoaded: item.weatherModel = weatherModel.weather
    }

    Text {
        elide: Text.ElideMiddle
        font.pixelSize: Units.dp(14)
        font.family: calaosFont.fontFamily
        horizontalAlignment: Text.AlignHCenter
        font.weight: Font.Thin
        color: "#E7E7E7"

        text: "%1 °C".arg(weatherModel.weather.temperature)

        anchors {
            left: parent.left; leftMargin: Units.dp(32)
            right: parent.right; rightMargin: Units.dp(32)
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(30)
        }
    }
}
