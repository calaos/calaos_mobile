import QtQuick
import SharedComponents

CalaosItemBase {
    id: thisButton

    property bool loadingEnabled: false

    Canvas {
        id: canvas

        anchors {
            fill: thisButton
            margins: 4 * calaosApp.density
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        renderStrategy: Canvas.Threaded
        antialiasing: true
        onPaint: drawSpinner();

        opacity: 0
        visible: opacity > 0

        property real arcEndPoint: 0
        property real arcStartPoint: 0
        property real rotate: 0
        property real longDash: 3 * Math.PI / 2
        property real shortDash: 19 * Math.PI / 10
        onArcEndPointChanged: requestPaint()
        onArcStartPointChanged: requestPaint()
        onRotateChanged: requestPaint()

        NumberAnimation {
            target: canvas
            properties: "rotate"
            from: 0
            to: 2 * Math.PI
            loops: Animation.Infinite
            running: canvas.visible
            easing.type: Easing.Linear
            duration: 3000
        }

        SequentialAnimation {
            running: canvas.visible
            loops: Animation.Infinite

            ParallelAnimation {
                NumberAnimation {
                    target: canvas
                    properties: "arcEndPoint"
                    from: 0
                    to: canvas.longDash
                    easing.type: Easing.InOutCubic
                    duration: 800
                }

                NumberAnimation {
                    target: canvas
                    properties: "arcStartPoint"
                    from: canvas.shortDash
                    to: 2 * Math.PI - 0.001
                    easing.type: Easing.InOutCubic
                    duration: 800
                }
            }

            ParallelAnimation {
                NumberAnimation {
                    target: canvas
                    properties: "arcEndPoint"
                    from: canvas.longDash
                    to: 2 * Math.PI - 0.001
                    easing.type: Easing.InOutCubic
                    duration: 800
                }

                NumberAnimation {
                    target: canvas
                    properties: "arcStartPoint"
                    from: 0
                    to: canvas.shortDash
                    easing.type: Easing.InOutCubic
                    duration: 800
                }
            }
        }

        function drawSpinner() {
            var ctx = canvas.getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            ctx.strokeStyle = "#3AB4D7"
            ctx.lineWidth = 2 * calaosApp.density
            ctx.lineCap = "butt";

            ctx.translate(canvas.width / 2, canvas.height / 2);
            ctx.rotate(canvas.rotate * (3 * Math.PI / 2));

            ctx.arc(0, 0, Math.min(canvas.width, canvas.height) / 2 - ctx.lineWidth,
                canvas.arcStartPoint,
                canvas.arcEndPoint,
                false);

            ctx.stroke();
        }
    }

    onLoadingEnabledChanged: {
        if (loadingEnabled)
            startAnim()
        else
            stopAnim()
    }

    function startAnim() {
        animStart.start()
    }

    function stopAnim() {
        animStart.stop()
        animReset.start()
    }

    SequentialAnimation {
        id: animStart
        ScriptAction { script: thisButton.state = "normal" }
        PropertyAnimation { target: __rectBorder; duration: 250; property: "radius"; to: height / 2; easing.type: Easing.OutCubic }
        ParallelAnimation {
            PropertyAnimation { target: __label; duration: 250; properties: "opacity"; to: 0; easing.type: Easing.OutCubic }
            PropertyAnimation { target: __rectBorder; duration: 250; properties: "width"; to: height; easing.type: Easing.OutCubic }
            PropertyAnimation { target: canvas; duration: 250; properties: "opacity"; to: 1; easing.type: Easing.OutCubic }
        }
    }

    SequentialAnimation {
        id: animReset
        PropertyAnimation { target: canvas; duration: 100; properties: "opacity"; to: 0; easing.type: Easing.InCubic }
        ParallelAnimation {
            PropertyAnimation { target: __rectBorder; duration: 250; property: "radius"; to: 8 * calaosApp.density; easing.type: Easing.InCubic }
            PropertyAnimation { target: __label; duration: 250; properties: "opacity"; to: 1; easing.type: Easing.InCubic }
            PropertyAnimation { target: __rectBorder; duration: 250; properties: "width"; to: parent.width; easing.type: Easing.InCubic }
        }
    }
}
