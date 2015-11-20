import QtQuick 2.4

MouseArea {
    id: thisButton

    property alias text: label.text
    property bool loadingEnabled: false

    signal buttonClicked();

    height: 50 * calaosApp.density

    Rectangle {
        id: rectBorder
        radius: 8 * calaosApp.density
        border.color: "#3AB4D7"
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

    Canvas {
        id: canvas

        anchors {
            fill: rectBorder
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

    Text {
        id: label
        font { family: calaosFont.fontFamilyThin; pointSize: 12 }
        anchors.centerIn: parent
        color: "#3AB4D7"
    }

    onLoadingEnabledChanged: {
        if (loadingEnabled)
            startAnim()
        else
            stopAnim()
    }

    function startAnim() {
        animStart.start()
        thisButton.enabled = false
    }

    function stopAnim() {
        animStart.stop()
        animReset.start()
    }

    hoverEnabled: enabled
    onEntered: state = "hovered"
    onExited: state = "normal"
    onClicked: buttonClicked()
    onPressed: inBorder.state = "pressed"
    onReleased: inBorder.state = "released"

    SequentialAnimation {
        id: animStart
        ScriptAction { script: thisButton.state = "normal" }
        PropertyAnimation { target: rectBorder; duration: 250; property: "radius"; to: height / 2; easing.type: Easing.OutCubic }
        ParallelAnimation {
            PropertyAnimation { target: label; duration: 250; properties: "opacity"; to: 0; easing.type: Easing.OutCubic }
            PropertyAnimation { target: rectBorder; duration: 250; properties: "width"; to: height; easing.type: Easing.OutCubic }
            PropertyAnimation { target: canvas; duration: 250; properties: "opacity"; to: 1; easing.type: Easing.OutCubic }
        }
    }

    SequentialAnimation {
        id: animReset
        PropertyAnimation { target: canvas; duration: 100; properties: "opacity"; to: 0; easing.type: Easing.InCubic }
        ParallelAnimation {
            PropertyAnimation { target: rectBorder; duration: 250; property: "radius"; to: 8 * calaosApp.density; easing.type: Easing.InCubic }
            PropertyAnimation { target: label; duration: 250; properties: "opacity"; to: 1; easing.type: Easing.InCubic }
            PropertyAnimation { target: rectBorder; duration: 250; properties: "width"; to: parent.width; easing.type: Easing.InCubic }
        }
        //enable mouse area again after anim finishes
        ScriptAction { script: thisButton.enabled = true }
    }

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
