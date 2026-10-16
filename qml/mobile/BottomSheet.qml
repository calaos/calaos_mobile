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
    //System insets of the window (home indicator, navigation bar, landscape
    //notch): the sheet's colour runs under them, its content does not.
    readonly property real insetTop: parent ? parent.SafeArea.margins.top : 0
    readonly property real insetBottom: parent ? parent.SafeArea.margins.bottom : 0
    readonly property real insetLeft: parent ? parent.SafeArea.margins.left : 0
    readonly property real insetRight: parent ? parent.SafeArea.margins.right : 0

    width: parent ? parent.width : 0
    //Never taller than the window below the status bar / Dynamic Island
    readonly property real maxHeight: parent ? Math.min(parent.height * 0.9,
                                                        parent.height - insetTop - Units.dp(16))
                                             : 0
    height: !parent ? 0
            : heightRatio > 0 ? Math.min(maxHeight, parent.height * heightRatio)
            : Math.min(maxHeight, headerHeight + fitHeight + Units.dp(24) + insetBottom)

    readonly property real headerHeight: Units.dp(14) + Units.dp(4) + Units.dp(14) +
                                         (titleText.visible ? titleText.height + Units.dp(8) : 0)

    topPadding: 0
    leftPadding: Units.dp(16) + insetLeft
    rightPadding: Units.dp(16) + insetRight
    bottomPadding: Units.dp(24) + insetBottom

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
