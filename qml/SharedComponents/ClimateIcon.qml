import QtQuick
import QtQuick.Shapes

//Line icon on a 24-unit grid (stroke 2), drawn as paths: the app fonts have no
//glyph for these and the panel client cannot rely on one either.
Item {
    id: root

    property string name: "help"
    property color color: "#e7e7e7"
    property real size: 14

    width: size
    height: size

    readonly property var paths: ({
        flame: "M12 3c1 3 4 4.5 4 8.5a4 4 0 0 1-8 0c0-1.5.6-2.6 1.4-3.6.3 1.3 1 2 1.9 2.3C11.6 8.6 11.3 5.5 12 3z M8.5 14.5c-.9 1-1.5 2.1-1.5 3.3A5 5 0 0 0 17 18c0-1.3-.6-2.4-1.5-3.4",
        snow: "M12 2v20M4.2 7l15.6 10M4.2 17L19.8 7 M12 2l-2.5 2.5M12 2l2.5 2.5M12 22l-2.5-2.5M12 22l2.5-2.5M4.2 7l3.4.9M4.2 7l.9-3.4M19.8 17l-3.4-.9M19.8 17l-.9 3.4M4.2 17l.9 3.4M4.2 17l3.4-.9M19.8 7l-3.4.9M19.8 7l-.9-3.4",
        autoArrows: "M7 20V5M4 8l3-3 3 3 M17 4v15M14 16l3 3 3-3",
        fan: "M10 12a2 2 0 1 0 4 0a2 2 0 1 0-4 0 M12 10c0-4-2-6-4-6s-3 2-1 4c1.5 1.5 3.5 2 5 2zM14 12c4 0 6-2 6-4s-2-3-4-1c-1.5 1.5-2 3.5-2 5zM12 14c0 4 2 6 4 6s3-2 1-4c-1.5-1.5-3.5-2-5-2zM10 12c-4 0-6 2-6 4s2 3 4 1c1.5-1.5 2-3.5 2-5z",
        drop: "M12 3s6 7 6 11.5a6 6 0 0 1-12 0C6 10 12 3 12 3z",
        power: "M12 3v9 M6.5 6.5a8 8 0 1 0 11 0",
        help: "M3 12a9 9 0 1 0 18 0a9 9 0 1 0-18 0 M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.7.3-1 .8-1 1.7 M12 17h.01",
        chevronR: "M9 6l6 6-6 6",
        chevronD: "M6 9l6 6 6-6",
        check: "M5 12l4 4L19 7"
    })

    Shape {
        objectName: "iconShape"
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft

        //Curve renderer: anti-aliased strokes without MSAA. Older Qt does not have it.
        Component.onCompleted: {
            if (Shape.CurveRenderer !== undefined)
                preferredRendererType = Shape.CurveRenderer
        }

        ShapePath {
            strokeColor: root.color
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin

            PathSvg { path: root.paths[root.name] !== undefined ? root.paths[root.name] : root.paths.help }
        }
    }
}
