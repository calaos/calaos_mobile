import QtQuick 2.5
import SharedComponents 1.0

Item {
    id: clock

    property variant widgetModel
    property size minimumSize: bg.sourceSize
    property size maximumSize: bg.sourceSize

    width: bg.width
    height: bg.height

    property int hours
    property int minutes
    property int seconds
    property real shift
    property bool internationalTime: false //Unset for local time

    function timeChanged() {
        var date = new Date;
        hours = internationalTime ? date.getUTCHours() + Math.floor(clock.shift) : date.getHours()
        minutes = internationalTime ? date.getUTCMinutes() + ((clock.shift % 1) * 60) : date.getMinutes()
        seconds = date.getUTCSeconds();
        textClock.text = date.toTimeString()
    }

    Fonts { id: calaosFont }

    Timer {
        interval: 100; running: true; repeat: true;
        onTriggered: clock.timeChanged()
    }

    Image {
        id: bg
        source: "widget_clock_back.png"
    }

    Text {
        id: textClock
        anchors {
            centerIn: parent
            verticalCenterOffset: Units.dp(25)
        }
        color: "#efefef"
        font { family: calaosFont.fontFamily; pixelSize: Units.dp(10); weight: Font.Thin }
        opacity: 0.8
    }

    Item {
        anchors.centerIn: parent
        width: bg.width
        height: bg.height

        Image {
            source: "clock_hour.png"
            transform: Rotation {
                id: hourRotation
                origin.x: bg.width / 2
                origin.y: bg.height / 2
                angle: (clock.hours * 30) + (clock.minutes * 0.5)
                Behavior on angle {
                    SpringAnimation { spring: 2; damping: 0.2; modulus: 360 }
                }
            }
        }

        Image {
            source: "clock_min.png"
            transform: Rotation {
                id: minuteRotation
                origin.x: bg.width / 2
                origin.y: bg.height / 2
                angle: clock.minutes * 6
                Behavior on angle {
                    SpringAnimation { spring: 2; damping: 0.2; modulus: 360 }
                }
            }
        }

        Image {
            source: "clock_sec.png"
            transform: Rotation {
                id: secondRotation
                origin.x: bg.width / 2
                origin.y: bg.height / 2
                angle: clock.seconds * 6
                Behavior on angle {
                    SpringAnimation { spring: 2; damping: 0.2; modulus: 360 }
                }
            }
        }
    }

}
