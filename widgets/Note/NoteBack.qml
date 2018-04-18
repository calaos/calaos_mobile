import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.3
import QuickFlux 1.0
import "qrc:/qml/quickflux"

Rectangle {

    property variant widgetModel
    property string noteText

    signal closeClicked

    color: "#333333"

    clip: true

    BorderImage {
        source: "shadow.png"
        anchors.fill: parent
        border.left: Units.dp(20)
        border.right: Units.dp(20)
        border.top: Units.dp(20)
        border.bottom: Units.dp(20)
    }

    Flickable {
        id: flickable
        anchors {
            top: parent.top; topMargin: Units.dp(20)
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        width: Units.dp(200)

        contentWidth: width
        contentHeight: col.implicitHeight

        ColumnLayout {
            id: col
            width: parent.width

            spacing: Units.dp(7)

            CalaosButton {
                text: qsTr("Edit note")
                onButtonClicked: AppActions.openKeyboard(qsTr("Note"),
                                                         qsTr("Write your note to be displayed on the desktop"),
                                                         noteText,
                                                         true,
                                                         ActionTypes.changeNoteText,
                                                         { uuid: widgetModel.uuid })
                hoverEnabled: false
                Layout.fillWidth: true
            }

            CalaosButton {
                text: qsTr("Send by mail")
                onButtonClicked: console.log("todo")
                hoverEnabled: false
                Layout.fillWidth: true
            }

            Rectangle {
                color: "#e7e7e7"
                opacity: 0.5
                Layout.preferredHeight: Units.dp(1)
                Layout.preferredWidth: Units.dp(80)
                Layout.alignment: Qt.AlignHCenter
            }

            CalaosButton {
                text: qsTr("Close")
                onButtonClicked: closeClicked()
                hoverEnabled: false
                Layout.fillWidth: true
            }
        }

        ScrollBar { listObject: flickable }
    }
}
