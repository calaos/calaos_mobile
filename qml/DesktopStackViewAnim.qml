import QtQuick 2.5
import QtQuick.Controls 1.4
import Units 1.0

/* Simple fade transition for now */
/* TODO: implement a better animations for all views */

StackViewDelegate {
    function transitionFinished(properties) {
        properties.exitItem.opacity = 1
    }

    pushTransition: StackViewTransition {
        PropertyAnimation {
            target: enterItem
            property: "opacity"
            from: 0
            to: 1
        }
        PropertyAnimation {
            target: enterItem
            property: "y"
            from: Units.dp(-10)
            to: 0
        }
        PropertyAnimation {
            target: exitItem
            property: "opacity"
            from: 1
            to: 0
        }
        PropertyAnimation {
            target: exitItem
            property: "y"
            from: Units.dp(4)
            to: 0
        }
    }

    popTransition: StackViewTransition {
        PropertyAnimation {
            target: enterItem
            property: "opacity"
            from: 0
            to: 1
        }
        PropertyAnimation {
            target: exitItem
            property: "opacity"
            from: 1
            to: 0
        }
        PropertyAnimation {
            target: exitItem
            property: "y"
            from: 0
            to: Units.dp(-10)
        }
    }
}
