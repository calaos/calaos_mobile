import QtQuick
import QtQuick.Controls as Controls
// ColorOverlay has no MultiEffect equivalent: the tint is half-transparent
// white, i.e. a 50/50 blend towards white that keeps the source hue.
// MultiEffect blends towards tint * luminance instead. See T29.
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import SharedComponents

Controls.TextField {
    id: control

    placeholderTextColor: "transparent"
    selectedTextColor: "#3AB4D7"
    selectionColor: Qt.rgba(58, 180, 215, 0.15)
    color: inlineField ? "transparent" : inlineTextColor
    passwordCharacter: "\u2022"
    activeFocusOnPress: !inlineField

    property bool showClearButton: true
    signal clearButtonClicked()

    //"username" or "password": on a platform with inline text fields, a real
    //native field sits on this one, the only kind the password manager
    //fills. This item keeps the text, the label, the underline and the clear
    //button; the native field shows the text and writes it here.
    property string inlineContentType: ""
    property string inlinePurpose: ""
    readonly property bool inlineField: inlineContentType !== "" && hardwareUtils.hasInlineTextFields()
    readonly property color inlineTextColor: "#e7e7e7"
    readonly property real inlineRightInset: iconClear.visible ? iconClear.width : 0
    //Set by the native field.
    property bool inlineEditing: false
    property real inlineKeyboardOverlap: 0
    signal inlineReturnPressed()
    signal inlineFocusRequested()

    readonly property bool editing: activeFocus || inlineEditing

    Component.onCompleted: {
        if (inlineField)
            hardwareUtils.bindInlineTextField(control, inlinePurpose, inlineContentType)
    }

    background: Item {
        id: background

        property color color: "#3AB4D7"
        property color errorColor: "#3AB4D7"
        property string helperText
        property bool floatingLabel: true
        property bool hasError: characterLimit && length > characterLimit
        property int characterLimit
        property bool showBorder: true

        Rectangle {
            id: underline
            color: background.hasError ? background.errorColor
                                    : control.editing ? background.color
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

            anchors.verticalCenter: parent.verticalCenter
            text: control.placeholderText
            font.pixelSize: Units.dp(16)
            anchors.margins: Units.dp(-12)
            color: background.hasError ? background.errorColor
                                  : control.editing && control.text !== ""
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
        opacity: parent.text != ""?1:0
        visible: opacity > 0 && showClearButton
        Behavior on opacity { NumberAnimation { duration: 100 } }
        anchors {
            right: parent.right
            top: parent.top
            bottom: parent.bottom
        }
        MouseArea {
            anchors.fill: parent
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

    ColorOverlay {
        id: overlay

        anchors.fill: iconClear
        source: iconClear
        color: Qt.rgba(1,1,1,0.5)
        cached: true
        visible: iconClear.visible
        opacity: iconClear.opacity
    }
}
