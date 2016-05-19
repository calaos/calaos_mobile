import QtQuick 2.5
import QtQuick.Layouts 1.1
import Calaos 1.0
import SharedComponents 1.0
import QtGraphicalEffects 1.0

Flipable {
    id: flipable

    function showMessage(title, msg) {
        titleitem.text = title;
        msgitem.text = msg;

        flipable.state = "back"
        closeTimeout.restart()
    }

    Timer {
        id: closeTimeout
        interval: 5000
        running: false
        repeat: false
        onTriggered: flipable.state = ""
    }

    transform: Rotation {
        id: rotation
        origin.x: flipable.width / 2
        origin.y: flipable.height
        axis.x: 1; axis.y: 0; axis.z: 0
        angle: 0
    }

    states: State {
        name: "back"
        PropertyChanges { target: rotation; angle: 180 }
        when: flipable.flipped
    }
    state: ""

    transitions: Transition {
        NumberAnimation { target: rotation; property: "angle"; easing.period: 0.5; easing.amplitude: 1.8
            easing.type: Easing.InOutBack; duration: 900 }
    }

    anchors.topMargin: -height
    width: notif.width
    height: notif.height

    back: Rectangle {
        id: notif

        radius: Units.dp(3)

        width: row.implicitWidth + 20
        height: col.implicitHeight + 20

        color: "#2a4255"

        MouseArea {
            anchors.fill: parent
            onClicked: flipable.state = ""
        }

        RowLayout {
            id: row

            height: Math.max(col.height, icon.height)
            anchors.centerIn: parent

            Item {
                id: icon
                width: iconReal.width
                height: iconReal.height
                Layout.alignment: Qt.AlignTop

                Image {
                    id: iconReal
                    source: "qrc:/img/info_outline.svg"
                    visible: false
                }

                ColorOverlay {
                    id: overlay

                    anchors.fill: parent
                    source: iconReal
                    color: "#efefef"
                    cached: true
                }
            }

            ColumnLayout {
                id: col

                Text {
                    id: titleitem
                    text: "Test title"

                    Layout.preferredWidth: Units.dp(200)
                    Layout.maximumWidth: Units.dp(200)

                    font.pixelSize: Units.dp(16)
                    color: "#34B4D7"
                    font.family: calaosFont.fontFamily
                    wrapMode: Text.WordWrap
                }

                Text {
                    id: msgitem
                    text: "Message lorem ipsum blabla blabla jhlkjsdlk jlk jdflkdjsf lkjsdlk fjlskdjf l kjdslfkj ldskjflskdjflkjds flkjd slfj dfdf."

                    width: col.width
                    Layout.preferredWidth: Units.dp(200)
                    Layout.maximumWidth: Units.dp(200)

                    font.pixelSize: Units.dp(14)
                    color: "#efefef"
                    font.family: calaosFont.fontFamily
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
