import QtQuick 2.5
import SharedComponents 1.0
import QuickFlux 1.0
import "qrc:/qml/quickflux"

Flipable {
    id: flipableNote

    property alias widgetModel: frontNote.widgetModel
    property alias minimumSize: frontNote.minimumSize
    property alias maximumSize: frontNote.maximumSize

    property bool flipped: false

    height: frontNote.height
    width: frontNote.width

    front: NoteFront {
        id: frontNote
        onSettingsClicked: flipableNote.flipped = true
    }

    back: NoteBack {
        widgetModel: frontNote.widgetModel
        noteText: frontNote.noteText

        height: frontNote.height
        width: frontNote.width
        onCloseClicked: flipableNote.flipped = false
    }

    transform: Rotation {
        id: rotation
        origin.x: flipableNote.width / 2
        origin.y: flipableNote.height / 2
        axis.x: 0; axis.y: 1; axis.z: 0
        angle: 0
    }

    states: State {
        name: "back"
        PropertyChanges { target: rotation; angle: 180 }
        when: flipableNote.flipped
    }

    transitions: Transition {
        NumberAnimation { target: rotation; property: "angle"; easing.type: Easing.OutCubic; duration: 500 }
    }

    AppListener {
        Filter {
            type: ActionTypes.changeNoteText
            onDispatched: (filtertype, message) => {
                console.log("note text update")

                if (message.returnPayload.uuid !== widgetModel.uuid) {
                    return; //not for us
                }

                frontNote.noteText = message.text

                var data = { noteText: message.text }
                widgetModel.saveData(JSON.stringify(data))
                flipableNote.flipped = false
            }
        }
    }
}
