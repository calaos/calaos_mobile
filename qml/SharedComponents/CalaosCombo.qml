import QtQuick
import QtQuick.Controls as Controls
import "."

Controls.ComboBox {
    id: control

    implicitHeight: Units.dp(40)
    implicitWidth: Units.dp(120)

    delegate: Controls.ItemDelegate {
        id: delegate

        required property var model
        required property int index

        width: control.width
        contentItem: Text {
            text: delegate.model[control.textRole]
            color: Theme.whiteColor
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: control.highlightedIndex === index

        background: Rectangle {
            color: delegate.highlighted ? Theme.colorAlpha(Theme.blueColor, 0.40) : Theme.backgroundColor
        }
    }

    indicator: Canvas {
        id: canvas
        x: control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 12
        height: 8
        contextType: "2d"

        Connections {
            target: control
            function onPressedChanged() { canvas.requestPaint(); }
        }

        onPaint: {
            context.reset();
            context.moveTo(0, 0);
            context.lineTo(width, 0);
            context.lineTo(width / 2, height);
            context.closePath();
            context.fillStyle = control.pressed ? Theme.blueColor : Theme.colorAlpha(Theme.whiteColor, 0.40);
            context.fill();
        }
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: control.indicator.width + control.spacing

        text: control.displayText
        font: control.font
        color: control.pressed ? Theme.blueColor : Theme.whiteColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 40
        border.color: Theme.blueColor
        border.width: Units.dp(2)
        radius: Units.dp(4)
        color: Theme.backgroundColor
    }

    popup: Controls.Popup {
        y: control.height - 1
        width: control.width
        implicitHeight: contentItem.implicitHeight
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            Controls.ScrollIndicator.vertical: Controls.ScrollIndicator { }
        }

        background: Rectangle {
            border.color: Theme.blueColor
            radius: Units.dp(2)
            color: Theme.backgroundColor
        }
    }
}
