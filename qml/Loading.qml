import QtQuick 2.0

Item {
    anchors.fill: parent
    id: loader

    visible: opacity > 0?true:false
    Behavior on opacity { PropertyAnimation { } }

    Rectangle {
        color: "#1f3e47"
        opacity: 0.50
        anchors.fill: parent
    }

    Rectangle {
        color: "black"
        radius: 30
        smooth: true

        width: 200
        height: 200

        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }

        Image {
            id: img
            source: "qrc:/img/logo_simple.png"
            anchors.centerIn: parent

            SequentialAnimation on opacity {
                running: loader.visible
                loops: Animation.Infinite
                NumberAnimation {
                    easing.type: Easing.InQuad
                    from: 0.25; to: 1
                    duration: 600
                }
                NumberAnimation {
                    easing.type: Easing.OutQuad
                    from: 1; to: 0.25
                    duration: 600
                }
            }
        }

        Text {
            anchors {
                horizontalCenter: parent.horizontalCenter
                top: img.bottom; topMargin: 15
            }
            text: qsTr("Loading")
            font { bold: true; pointSize: 12 }
            color: "#E7E7E7"
        }

    }
}
