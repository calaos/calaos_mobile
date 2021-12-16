import QtQuick
import QtQuick.Controls
import SharedComponents

/* Simple fade transition for now */
/* TODO: implement a better animations for all views */

StackView {

    //    function transitionFinished(properties) {
    //        properties.exitItem.opacity = 1
    //    }

    pushEnter: Transition {
        ParallelAnimation {
            PropertyAnimation {
                property: "opacity"
                from: 0
                to: 1
            }
            PropertyAnimation {
                property: "y"
                from: Units.dp(-10)
                to: 0
            }
        }
    }

    pushExit: Transition {
        ParallelAnimation {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
            }
            PropertyAnimation {
                property: "y"
                from: Units.dp(4)
                to: 0
            }
        }
    }

    popEnter: Transition {
        PropertyAnimation {
            property: "opacity"
            from: 0
            to: 1
        }
    }

    popExit: Transition {
        ParallelAnimation {
            PropertyAnimation {
                property: "opacity"
                from: 1
                to: 0
            }
            PropertyAnimation {
                property: "y"
                from: 0
                to: Units.dp(-10)
            }
        }
    }

}
