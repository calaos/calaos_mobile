import QtQuick 2.5
import SharedComponents 1.0
import QuickFlux 1.0
import "qrc:/qml/quickflux"

Item {
    id: note

    property variant widgetModel
    property size minimumSize: bg.sourceSize
    property size maximumSize: Qt.size(Units.dp(1024), Units.dp(768))

    property alias noteText: noteTxt.text

    signal settingsClicked

    width: bg.width
    height: bg.height

    Fonts { id: calaosFont }

    onWidgetModelChanged: {
        //load the data from disk
        var data = JSON.parse(widgetModel.getData())
        noteTxt.text = data.noteText
    }

    BorderImage {
        id: bg
        source: "note_back.png"
        border.left: Units.dp(51); border.top: Units.dp(53)
        border.right: Units.dp(154); border.bottom: Units.dp(97)

        width: widgetModel.width <= 0? bg.sourceSize.width: widgetModel.width
        height: widgetModel.height <= 0? bg.sourceSize.height: widgetModel.height

        Item {
            anchors {
                fill: parent
                leftMargin: Units.dp(52)
                rightMargin: Units.dp(64)
                topMargin: Units.dp(50)
                bottomMargin: Units.dp(35)
            }

            Flickable {
                id: flickable
                anchors.fill: parent

                contentWidth: width
                contentHeight: noteTxt.implicitHeight

                Text {
                    id: noteTxt
                    wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                    width: flickable.width
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
                }
            }

            ScrollBar { listObject: flickable }

            clip: true
        }
    }

    Image {
        source: "settings.svg"
        anchors {
            top: parent.top; topMargin: Units.dp(24)
            left: parent.left; leftMargin: Units.dp(44)
        }
        width: Units.dp(18)
        height: Units.dp(18)
        opacity: 0.65

        MouseArea {
            anchors.fill: parent
            anchors.margins: Units.dp(-5)
            onClicked: settingsClicked()
        }
    }
}
