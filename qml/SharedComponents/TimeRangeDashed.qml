import QtQuick
import QtQuick.Shapes
import "."

//Rounded box with a dashed border: the "add" row, the empty day and the pause
//banner. PathSvg rather than PathRectangle, which older Qt does not have.
Item {
    id: root

    property color color: Theme.blueColor
    property color fillColor: "transparent"
    property real radius: Units.dp(6)

    Shape {
        anchors.fill: parent

        ShapePath {
            strokeColor: root.color
            strokeWidth: Units.dp(1)
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            fillColor: root.fillColor

            PathSvg {
                readonly property real w: root.width - 1
                readonly property real h: root.height - 1
                readonly property real r: Math.min(root.radius, w / 2, h / 2)
                path: "M " + (0.5 + r) + " 0.5 H " + (w + 0.5 - r)
                      + " A " + r + " " + r + " 0 0 1 " + (w + 0.5) + " " + (0.5 + r)
                      + " V " + (h + 0.5 - r)
                      + " A " + r + " " + r + " 0 0 1 " + (w + 0.5 - r) + " " + (h + 0.5)
                      + " H " + (0.5 + r)
                      + " A " + r + " " + r + " 0 0 1 0.5 " + (h + 0.5 - r)
                      + " V " + (0.5 + r)
                      + " A " + r + " " + r + " 0 0 1 " + (0.5 + r) + " 0.5 Z"
            }
        }
    }
}
