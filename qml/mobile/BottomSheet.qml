import QtQuick
import QtQuick.Controls
import Calaos
import SharedComponents

//Sheet that slides up from the bottom edge, in the colours of Dialog.qml:
//60 % black veil, #171717 body, a grip. A second sheet opened on top leaves
//this one visible, darkened, behind it.
Drawer {
    id: sheet

    //Share of the window height, or 0 to fit the content
    property real heightRatio: 0
    property string title
    property string fontFamily

    //With no ratio the sheet is as tall as its title and this
    property real fitHeight: 0

    default property alias sheetContent: body.data

    parent: Overlay.overlay
    edge: Qt.BottomEdge
    modal: true
    dim: true
    //A swipe from the edge of the screen must not open an empty sheet: it is
    //opened by code only. Swiping it down still closes it.
    dragMargin: 0
    //Back and Escape are taken on release, as Dialog.qml does: closing on the
    //press lets the release reach the page below, which then goes back too.
    closePolicy: Popup.CloseOnPressOutside
    focus: true
    width: parent ? parent.width : 0
    height: !parent ? 0
            : heightRatio > 0 ? parent.height * heightRatio
            : Math.min(parent.height * 0.9, headerHeight + fitHeight + Units.dp(24))

    readonly property real headerHeight: Units.dp(14) + Units.dp(4) + Units.dp(14) +
                                         (titleText.visible ? titleText.height + Units.dp(8) : 0)

    topPadding: 0
    leftPadding: Units.dp(16)
    rightPadding: Units.dp(16)
    bottomPadding: Units.dp(24)

    //The sheet lives on the window overlay, outside the pages: going back to the
    //login page does not take it away. Every sheet of the app goes through here.
    Connections {
        target: calaosApp
        function onApplicationStatusChanged(status) {
            if (status !== Common.LoggedIn)
                sheet.close()
        }
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }

    background: Rectangle {
        color: Theme.backgroundColor
        radius: Units.dp(16)

        //Only the top corners are round
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: parent.radius
            color: parent.color
        }
    }

    contentItem: Item {
        focus: true

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape)
                event.accepted = true
        }

        Keys.onReleased: (event) => {
            if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
                sheet.close()
                event.accepted = true
            }
        }

        Rectangle {
            id: grip
            objectName: "grip"
            anchors { top: parent.top; topMargin: Units.dp(10); horizontalCenter: parent.horizontalCenter }
            width: Units.dp(40)
            height: Units.dp(4)
            radius: height / 2
            color: Theme.mutedColor
        }

        Text {
            id: titleText
            objectName: "sheetTitle"
            anchors { top: grip.bottom; topMargin: Units.dp(14); left: parent.left }
            visible: sheet.title !== ""
            color: Theme.whiteColor
            font { family: sheet.fontFamily; pixelSize: Units.dp(20) }
            text: sheet.title
        }

        Item {
            id: body
            anchors {
                top: titleText.visible ? titleText.bottom : grip.bottom
                topMargin: titleText.visible ? Units.dp(8) : Units.dp(14)
                left: parent.left; right: parent.right; bottom: parent.bottom
            }
        }
    }
}
