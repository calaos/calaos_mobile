import QtQuick
import QtQuick.Layouts
import SharedComponents

PopupBase {
    id: dialog

    overlayLayer: "dialogOverlayLayer"
    overlayColor: Qt.rgba(0, 0, 0, 0.6)

    opacity: showing ? 1 : 0
    visible: opacity > 0

    width: Math.max(minimumWidth,
                    content.contentWidth + 2 * contentMargins)

    height: Math.min(parent.height - Units.dp(64),
                     headerView.height +
                     content.contentHeight +
                     (floatingActions ? 0 : buttonContainer.height))

    property int contentMargins: Units.dp(24)

    property int minimumWidth: Units.dp(400)

    property alias title: titleLabel.text
    property alias text: textLabel.text

    /*!
       \qmlproperty Button negativeButton
       The negative button, displayed as the leftmost button on the right of the dialog buttons.
       This is usually used to dismiss the dialog.
     */
    property alias negativeButton: negativeButton

    /*!
       \qmlproperty Button primaryButton
       The primary button, displayed as the rightmost button in the dialog buttons row. This is
       usually used to accept the dialog's action.
     */
    property alias positiveButton: positiveButton

    property string negativeButtonText: qsTr("Cancel")
    property string positiveButtonText: qsTr("Ok")
    property bool positiveButtonEnabled: true

    property bool hasActions: true
    property bool floatingActions: false

    property alias flickableContent: content.interactive

    default property alias dialogContent: column.data

    property color dialogBgColor: "#171717"

    signal accepted()
    signal rejected()

    anchors {
        centerIn: parent
        verticalCenterOffset: showing ? 0 : -(dialog.height/3)

        Behavior on verticalCenterOffset {
            NumberAnimation { duration: 200 }
        }
    }

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    Keys.onPressed: {
        if (event.key === Qt.Key_Escape) {
            closeKeyPressed(event)
        }
    }

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            closeKeyPressed(event)
        }
    }

    function closeKeyPressed(event) {
        if (dialog.showing) {
            if (dialog.dismissOnTap) {
                dialog.close()
            }
            event.accepted = true
        }
    }

    function show() {
        open()
    }

    Rectangle {
        id: dialogContainer

        anchors.fill: parent
        color: dialogBgColor

        MouseArea {
            anchors.fill: parent
            propagateComposedEvents: false

            onClicked: {
                mouse.accepted = false
            }
        }

        Flickable {
            id: content

            contentWidth: column.implicitWidth
            contentHeight: column.height + (column.height > 0 ? contentMargins : 0)
            clip: true

            anchors {
                left: parent.left
                right: parent.right
                top: headerView.bottom
                bottom: floatingActions ? parent.bottom : buttonContainer.top
            }

            interactive: contentHeight > height

            onContentXChanged: {
                if(contentX != 0 && contentWidth <= width)
                    contentX = 0
            }

            onContentYChanged: {
                if(contentY != 0 && contentHeight <= height)
                    contentY = 0
            }

            Column {
                id: column
                anchors {
                    left: parent.left
                    leftMargin: contentMargins
                }

                width: content.width - 2 * contentMargins
                spacing: Units.dp(8)
            }
        }

        ScrollBar {
            listObject: content
            visible: flickableContent
        }

        Rectangle {
            color: dialogBgColor
            clip: true
            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
            }

            height: headerView.height

            Image {
                source: "qrc:/img/background_quad.png"

                fillMode: Image.Tile
                horizontalAlignment: Image.AlignLeft
                verticalAlignment: Image.AlignTop
                anchors.fill: parent
            }

            Image {
                source: "qrc:/img/background_dialog.png"
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    bottom: parent.bottom
                }
            }
        }


        Column {
            id: headerView

            spacing: 0

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top

                leftMargin: contentMargins
                rightMargin: contentMargins
            }

            Item {
                width: parent.width
                height: contentMargins
                visible: titleLabel.visible || textLabel.visible
            }

            Text {
                id: titleLabel

                width: parent.width
                wrapMode: Text.Wrap
                visible: title != ""

                font.pixelSize: Units.dp(20)
                font.family: calaosFont.fontFamily
                font.weight: Font.Thin
                color: "#3AB4D7"
            }

            Item {
                width: parent.width
                height: Units.dp(20)
                visible: titleLabel.visible
            }

            Text {
                id: textLabel

                width: parent.width
                wrapMode: Text.Wrap
                visible: text != ""

                elide: Text.ElideMiddle
                font.pixelSize: Units.dp(14)
                font.family: calaosFont.fontFamily
                font.weight: Font.Thin
                color: "#E7E7E7"

            }

            Item {
                width: parent.width
                height: contentMargins
                visible: textLabel.visible
            }
        }

        Item {
            id: buttonContainer

            anchors {
                bottom: parent.bottom
                right: parent.right
                left: parent.left
            }

            height: hasActions ? Units.dp(52) : Units.dp(2)

            BasePopupView {
                id: buttonView

                height: parent.height
                backgroundColor: floatingActions ? "transparent" : dialogBgColor
                elevation: content.atYEnd ? 0 : 1
                fullWidth: true
                radius: dialogContainer.radius
                elevationInverted: true

                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    left: parent.left
                }

                CalaosButton {
                    id: negativeButton

                    width: (parent.width - Units.dp(30)) / 2

                    visible: hasActions
                    text: negativeButtonText

                    anchors {
                        verticalCenter: parent.verticalCenter
                        right: positiveButton.visible ? positiveButton.left : parent.right
                        rightMargin: Units.dp(8)
                    }

                    onClicked: {
                        close();
                        rejected();
                    }
                }

                CalaosButton {
                    id: positiveButton

                    width: (parent.width - Units.dp(30)) / 2

                    visible: hasActions && positiveButtonEnabled
                    text: positiveButtonText

                    anchors {
                        verticalCenter: parent.verticalCenter
                        rightMargin: Units.dp(8)
                        right: parent.right
                    }

                    onClicked: {
                        close()
                        accepted();
                    }
                }
            }

            Image {
                source: "qrc:/img/neon.png"

                anchors {
                    left: parent.left; right: parent.right
                    verticalCenter: parent.top
                }
            }
        }

        Rectangle {
            radius: Units.dp(4)
            border.color: "#3AB4D7"
            border.width: Units.dp(1)
            color: "transparent"
            opacity: 0.25

            anchors.fill: parent
        }
    }
}
