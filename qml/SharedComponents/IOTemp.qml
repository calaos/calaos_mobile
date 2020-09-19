import QtQuick 2.0
import "."
import QtQuick.Controls 2.12
import QtCharts 2.0

ItemBase {
    property variant modelData

    IconItem {
        id: icon

        source: calaosApp.getPictureSized("icon_temp")

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: name
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: temp.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    Text {
        id: temp
        color: "#ffda5a"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
        clip: true
        text: modelData.stateInt + " °C"
        elide: Text.ElideNone
        anchors {
            right: btMore.left; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
    }

    ItemButtonAction {
        id: btMore
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: parent.verticalCenter
        }
        imageSource: "button_info_glow"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
            popup.open()
        }
    }

    Popup {
        id: popup
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        anchors.centerIn: Overlay.overlay
        width: Overlay.overlay.width  - 8 * calaosApp.density
        height: Overlay.overlay.height * 2 / 3
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }
        ChartView {
            id: chartView
            //antialiasing: true
            anchors.fill: parent
            backgroundRoundness: 8
            dropShadowEnabled: true
            theme: ChartView.ChartThemeBlueCerulean
            LineSeries {
                id: tempSeries
                axisX: valueAxisX
                axisY: valueAxisY
                color: "#fab800"

            }
            DateTimeAxis {
                id: valueAxisX
                visible: true
                format: "ddd  hh:mm"
            }

            ValueAxis{
                id: valueAxisY
                visible: true
            }
        }
    }

    Timer {
        interval: 300000
        repeat: true
        triggeredOnStart: true
        running: true
        onTriggered: {
            var query = 'SELECT "value" FROM "' + modelData.ioName + '" WHERE time >= now() - 1d'
            var url = calaosApp.hostname;
            console.log(url)
            var xhr = new XMLHttpRequest;
            var data = {cn_user: calaosApp.username,
                cn_pass: calaosApp.password,
                action: "datalogger",
                q: query};
            console.log(data.q);
            xhr.open("POST",url);
            xhr.setRequestHeader('Content-Type', 'application/json')
            xhr.onreadystatechange = function() {
                if (xhr.readyState === XMLHttpRequest.DONE) {
                    console.log(xhr.responseText)
                    var a = JSON.parse(xhr.responseText);
                    //                        console.log(xhr.responseText)
                    parseData(a);
                }
            }
            xhr.send(JSON.stringify(data));
        }
    }

    function parseData(data) {
        tempSeries.clear();
        var xmax;
        var xmin;
        var ymax = -100;
        var ymin = 100
        console.log(data)
        for (var i in data.results) {
            for (var j in data.results[i].series) {
                tempSeries.name = data.results[i].series[j].name;
                for (var k in data.results[i].series[j].values) {
                    let x = new Date(data.results[i].series[j].values[k][0])
                    if (x > xmax)
                        xmax = x;
                    var y = data.results[i].series[j].values[k][1]
                    if (y > ymax)
                        ymax = y;
                    if (y < ymin)
                        ymin = y;
                    tempSeries.append(x, y)
                }
                var arr = data.results[i].series[j].values
                xmin = new Date(arr[0][0]);
                xmax = new Date(Date.now());
            }
        }
        valueAxisX.max = xmax;
        valueAxisX.min = xmin;
        valueAxisY.max = Math.ceil(ymax);
        valueAxisY.min = Math.floor(ymin);

    }

    Component.onCompleted: {
        chartTheme.update_chart(chartView)
        chartTheme.update_axes(valueAxisX, valueAxisY)
    }
}
