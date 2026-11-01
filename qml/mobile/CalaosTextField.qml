import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import SharedComponents

Controls.TextField {
    id: control

    placeholderTextColor: "transparent"
    selectedTextColor: "#3AB4D7"
    selectionColor: Qt.rgba(58, 180, 215, 0.15)
    color: "#e7e7e7"
    passwordCharacter: "\u2022"

    property bool showClearButton: true
    //A box around the field rather than a line under it, and no floating label
    property bool framed: false
    signal clearButtonClicked()

    leftPadding: framed ? Units.dp(12) : undefined
    rightPadding: framed ? Units.dp(44) : undefined

    background: Item {
        id: background

        property color color: "#3AB4D7"
        property color errorColor: "#3AB4D7"
        property string helperText
        property bool floatingLabel: !control.framed
        property bool hasError: characterLimit && length > characterLimit
        property int characterLimit
        property bool showBorder: !control.framed

        Rectangle {
            objectName: "fieldFrame"
            anchors.fill: parent
            visible: control.framed
            radius: Units.dp(8)
            color: "transparent"
            border {
                width: Units.dp(2)
                color: control.activeFocus ? background.color : "#424242"
            }
        }

        Rectangle {
            id: underline
            color: background.hasError ? background.errorColor
                                    : control.activeFocus ? background.color
                                                          : Qt.rgba(1, 1, 1, 0.3)

            height: Units.dp(2)
            visible: background.showBorder

            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            Behavior on height {
                NumberAnimation { duration: 200 }
            }

            Behavior on color {
                ColorAnimation { duration: 200 }
            }
        }

        Controls.Label {
            id: fieldPlaceholder

            x: control.framed ? Units.dp(12) : 0
            anchors.verticalCenter: parent.verticalCenter
            text: control.placeholderText
            font.pixelSize: Units.dp(16)
            anchors.margins: Units.dp(-12)
            color: background.hasError ? background.errorColor
                                  : control.activeFocus && control.text !== ""
                                        ? background.color : Qt.rgba(1, 1, 1, 0.3)

            states: [
                State {
                    name: "floating"
                    when: control.displayText.length > 0 && background.floatingLabel
                    AnchorChanges {
                        target: fieldPlaceholder
                        anchors.verticalCenter: undefined
                        anchors.top: parent.top
                    }
                    PropertyChanges {
                        target: fieldPlaceholder
                        font.pixelSize: Units.dp(12)
                    }
                },
                State {
                    name: "hidden"
                    when: control.displayText.length > 0 && !background.floatingLabel
                    PropertyChanges {
                        target: fieldPlaceholder
                        visible: false
                    }
                }
            ]

            transitions: [
                Transition {
                    id: floatingTransition
                    enabled: false
                    AnchorAnimation {
                        duration: 200
                    }
                    NumberAnimation {
                        duration: 200
                        property: "font.pixelSize"
                    }
                }
            ]

            Component.onCompleted: floatingTransition.enabled = true
        }

        RowLayout {
            anchors {
                left: parent.left
                right: parent.right
                top: underline.top
                topMargin: Units.dp(4)
            }

            Controls.Label {
                id: helperTextLabel
                visible: background.helperText && background.showBorder
                text: background.helperText
                font.pixelSize: Units.dp(12)
                color: background.hasError ? background.errorColor
                                           : Qt.darker(Qt.rgba(1, 1, 1, 0.3))

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }

                property string helperText: control.hasOwnProperty("helperText")
                        ? control.helperText : ""
            }

            Controls.Label {
                id: charLimitLabel
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                visible: background.characterLimit && background.showBorder
                text: control.length + " / " + background.characterLimit
                font.pixelSize: Units.dp(12)
                color: background.hasError ? background.errorColor : Qt.rgba(1, 1, 1, 0.3)
                horizontalAlignment: Text.AlignLeft

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }
    }

    Image {
        id: iconClear
        fillMode: Image.PreserveAspectFit
        source: "qrc:/img/close.svg"
        sourceSize: Qt.size(Units.dp(18), Units.dp(18))
        opacity: parent.text != ""?1:0
        visible: opacity > 0 && showClearButton
        Behavior on opacity { NumberAnimation { duration: 100 } }
        anchors {
            right: parent.right
            rightMargin: control.framed ? Units.dp(13) : 0
            top: parent.top
            bottom: parent.bottom
        }
        MouseArea {
            anchors {
                fill: parent
                leftMargin: -Units.dp(13)
                rightMargin: control.framed ? -Units.dp(13) : 0
            }
            onClicked: {
                // cut() is a no-op in Password echo mode, so clear explicitly.
                // Reset the IME first so a pending preedit cannot reappear.
                // Qt.inputMethod is seen as a plain QObject by the linter
                // qmllint disable missing-property
                Qt.inputMethod.reset()
                // qmllint enable missing-property
                control.clear()
                control.clearButtonClicked()
            }
        }
    }
}
