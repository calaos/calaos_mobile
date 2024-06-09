import QtQuick
import "."

MouseArea {
    id: thisButton

    property alias text: label.text
    property bool disabled: false

    enabled: !disabled

    signal buttonClicked()

    height: 50 * calaosApp.density

    property alias __rectBorder: rectBorder
    property alias __inBorder: inBorder
    property alias __label: label

    Rectangle {
        id: rectBorder
        radius: 8 * calaosApp.density
        border.color: thisButton.disabled? Theme.colorAlpha(Theme.whiteColor, 0.40) : "#3AB4D7"
        border.width: 2 * calaosApp.density
        color: "transparent"

        anchors {
            top: parent.top
            bottom: parent.bottom
            horizontalCenter: parent.horizontalCenter
        }
        width: parent.width

        opacity: 0.5
    }

    Rectangle {
        id: inBorder
        radius: 4 * calaosApp.density
        color: "#3AB4D7"

        anchors {
            fill: rectBorder
            margins: 4 * calaosApp.density
        }

        opacity: 0

        state: "released"

        states: [
            State { name: "released"; PropertyChanges { target: inBorder; opacity: 0.0 } },
            State { name: "pressed"; PropertyChanges { target: inBorder; opacity: 0.1 } }
        ]
        transitions: [
            Transition {
                from: "released"
                to: "pressed"
                PropertyAnimation { duration: 50; properties: "opacity"; easing.type: Easing.OutCubic }
            },
            Transition {
                from: "pressed"
                to: "released"
                PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.InCubic }
            }
        ]
    }

    Text {
        id: label
        font { family: calaosFont.fontFamilyThin; pointSize: 12 }
        anchors.centerIn: parent
        color: thisButton.disabled? Theme.colorAlpha(Theme.whiteColor, 0.40) : "#3AB4D7"
    }

    hoverEnabled: enabled
    onEntered: state = "hovered"
    onExited: state = "normal"
    onClicked: buttonClicked()
    onPressed: inBorder.state = "pressed"
    onReleased: inBorder.state = "released"

    state: "normal"
    states: [
        State {
            name: "normal"; PropertyChanges { target: rectBorder; opacity: 0.4 }
        },
        State {
            name: "hovered"; PropertyChanges { target: rectBorder; opacity: 0.85 }
        }
    ]

    transitions: [
        Transition {
            from: "normal"
            to: "hovered"
            PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.InCubic }
        },
        Transition {
            from: "hovered"
            to: "normal"
            PropertyAnimation { duration: 150; properties: "opacity"; easing.type: Easing.OutCubic }
        }
    ]
}
