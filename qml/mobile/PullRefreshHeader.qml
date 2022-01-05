import QtQuick
import SharedComponents
import Calaos

Item {
    height: Units.dp(60)
    width: parent.width

    property ListView listview

    property bool refresh: state == "pulled" ? true : false

    Row {
        spacing: Units.dp(6)
        height: childrenRect.height
        anchors.centerIn: parent

        Image {
            id: arrow
            fillMode: Image.PreserveAspectFit
            source: "qrc:/img/down_arrow.svg"
            transformOrigin: Item.Center
            Behavior on rotation { NumberAnimation { duration: 200 } }
            width: Units.dp(30)
            height: Units.dp(30)
        }

        Text {
            id: label
            anchors.verticalCenter: arrow.verticalCenter
            text: qsTr("Pull to refresh...")
            font { family: calaosFont.fontFamily; bold: false; pointSize: 12 }
            color: "#e7e7e7"
        }
    }

    states: [
        State {
            name: "base"; when: listview.contentY >= Units.dp(-50)
            PropertyChanges { target: arrow; rotation: 0 }
        },
        State {
            name: "pulled"; when: listview.contentY < Units.dp(-50)
            PropertyChanges { target: label; text: qsTr("Release to refresh...") }
            PropertyChanges { target: arrow; rotation: 180 }
        }
    ]
}
