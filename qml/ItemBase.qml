import QtQuick 2.3

BorderImage {
    id: baseItem

    source: calaosApp.getPictureSized("back_items_home")
    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

    width: parent.width

    //default 1 row height
    height: 40 * calaosApp.density

    function feedbackAnim() {
        baseItem.state = "pressed"
        animTimer.restart()
    }

    //Overlay image for feedback anim when a button is pressed
    BorderImage {
        id: overlay
        source: calaosApp.getPictureSized("back_items_home_glow")
        border.left: 10 * calaosApp.density; border.top: 10 * calaosApp.density
        border.right: 10 * calaosApp.density; border.bottom: 10 * calaosApp.density
        anchors.fill: parent

        opacity: 0
        visible: opacity > 0
    }

    Timer {
        id: animTimer
        interval: 300
        repeat: false
        running: false
        triggeredOnStart: false
        onTriggered: {
            baseItem.state = "released"
        }
    }

    state: "released" //default state is released
    states: [
        State {
            name: "pressed"
            PropertyChanges { target: overlay; opacity: 1 }
        },
        State {
            name: "released"
            PropertyChanges { target: overlay; opacity: 0 }
        }
    ]

    transitions: [
        Transition {
            from: "released"
            to: "pressed"
            PropertyAnimation { target: overlay; property: "opacity"; duration: 50; easing.type: Easing.InQuart  }
        },
        Transition {
            from: "pressed"
            to: "released"
            PropertyAnimation { target: overlay; property: "opacity"; duration: 500; easing.type: Easing.OutQuint  }
        }
    ]
}

