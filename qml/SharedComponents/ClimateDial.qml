pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import "ClimateLogic.js" as Logic

//Mini 300-degree dial of the list item. Everything is laid out on the 52-unit
//design grid and scaled to `size`; the bottom opening is cropped away so the
//item keeps its height for the text.
Item {
    id: root

    //Result of Logic.dialModel()
    property var model: ({ fill: null, range: null, knobs: [], tick: null })
    property real size: 52
    property color trackColor: Qt.rgba(1, 1, 1, 0.3)
    property color fillColor: "#868482"
    property color rangeColor: "#4a4a4a"
    property color tickColor: "#ffffff"
    //Backdrop of the item: the ring that separates a knob from the arc
    property color ringColor: "#0b0f12"
    property var knobColors: []

    //Proportions of the 52-unit mini dial; the large one sets its own
    readonly property real unit: size / 52
    property real stroke: 4.5 * unit
    property real knobRadius: 4.5 * unit
    property real tickWidth: 1.5 * unit
    property real tickReach: 0.9
    //The ring that separates a knob from the arc
    property real ringWidth: 2 * unit
    readonly property real pad: Math.max(knobRadius + ringWidth, stroke / 2) + ringWidth / 2
    readonly property real radius: size / 2 - pad
    readonly property real center: size / 2

    width: size
    height: Math.ceil(center + radius * Math.cos(Math.PI / 6) + pad)

    //Centred in the opening of the dial
    default property alias content: inner.data

    function polarX(angle) { return center + radius * Math.cos((angle - 90) * Math.PI / 180) }
    function polarY(angle) { return center + radius * Math.sin((angle - 90) * Math.PI / 180) }

    //Point on the ray at `angle`, `offset` away from the arc line; 0 when there is no tick
    function radial(angle, offset, isX) {
        if (angle === null)
            return 0
        var a = (angle - 90) * Math.PI / 180
        return center + (radius + offset) * (isX ? Math.cos(a) : Math.sin(a))
    }

    //A zero sweep draws nothing, a short one draws the round cap as a dot
    function sweep(from, to) { return Math.max(0.4, to - from) }

    Shape {
        objectName: "dialShape"
        anchors.fill: parent

        //Curve renderer: anti-aliased strokes without MSAA. Older Qt does not have it.
        Component.onCompleted: {
            if (Shape.CurveRenderer !== undefined)
                preferredRendererType = Shape.CurveRenderer
        }


        ShapePath {
            strokeColor: root.trackColor
            strokeWidth: root.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.center; centerY: root.center
                radiusX: root.radius; radiusY: root.radius
                startAngle: Logic.ARC_START - 90
                sweepAngle: Logic.ARC_SPAN
            }
        }

        ShapePath {
            objectName: "arcFill"
            strokeColor: root.model.fill ? root.fillColor : "transparent"
            strokeWidth: root.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.center; centerY: root.center
                radiusX: root.radius; radiusY: root.radius
                startAngle: (root.model.fill ? root.model.fill[0] : 0) - 90
                sweepAngle: root.model.fill ? root.sweep(root.model.fill[0], root.model.fill[1]) : 0
            }
        }

        ShapePath {
            objectName: "arcRange"
            strokeColor: root.model.range ? root.rangeColor : "transparent"
            strokeWidth: root.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: root.center; centerY: root.center
                radiusX: root.radius; radiusY: root.radius
                startAngle: (root.model.range ? root.model.range[0] : 0) - 90
                sweepAngle: root.model.range ? root.sweep(root.model.range[0], root.model.range[1]) : 0
            }
        }

        ShapePath {
            objectName: "arcTick"
            strokeColor: root.model.tick !== null ? root.tickColor : "transparent"
            strokeWidth: root.tickWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            startX: root.radial(root.model.tick, -root.stroke * root.tickReach, true)
            startY: root.radial(root.model.tick, -root.stroke * root.tickReach, false)
            PathLine {
                x: root.radial(root.model.tick, root.stroke * root.tickReach, true)
                y: root.radial(root.model.tick, root.stroke * root.tickReach, false)
            }
        }
    }

    Item {
        id: inner
        x: 0
        y: root.center
        width: root.width
        height: 0
    }

    Repeater {
        model: root.model.knobs.length

        delegate: Rectangle {
            required property int index

            objectName: "knob" + index
            //The ring is centred on the knob's edge, so the outline is one ring-width wider
            readonly property real outer: root.knobRadius + root.ringWidth / 2
            width: outer * 2
            height: width
            radius: outer
            x: root.polarX(root.model.knobs[index]) - outer
            y: root.polarY(root.model.knobs[index]) - outer
            color: root.knobColors[index] !== undefined ? root.knobColors[index] : "#ffffff"
            border.color: root.ringColor
            border.width: root.ringWidth
        }
    }
}
