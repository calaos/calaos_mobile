import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Controls.Universal
//import QtQuick.Controls.Styles
import QtQuick.Layouts
import SharedComponents

TextFieldStyle {
    readonly property TextField control: __control

    padding {
        left: 0
        right: 0
        top: 0
        bottom: 0
    }

    font {
        family: control.echoMode == TextInput.Password?
                    calaosFont.fontFamilyBold:
                    calaosFont.fontFamilyLight
        pixelSize: 16 * calaosApp.density
    }

    renderType: Text.QtRendering
    placeholderTextColor: "transparent"
    selectedTextColor: "#3AB4D7"
    selectionColor: Qt.rgba(58, 180, 215, 0.15)
    textColor: "#e7e7e7"
    passwordCharacter: "\u2022"

    background: Item {
        id: background

        property color color: control.hasOwnProperty("color") ? control.color : "#3AB4D7"
        property color errorColor: control.hasOwnProperty("errorColor")
                ? control.errorColor : "#F44336"
        property string helperText: control.hasOwnProperty("helperText") ? control.helperText : ""
        property bool floatingLabel: control.hasOwnProperty("floatingLabel") ? control.floatingLabel : ""
        property bool hasError: control.hasOwnProperty("hasError")
                ? control.hasError : characterLimit && control.length > characterLimit
        property int characterLimit: control.hasOwnProperty("characterLimit") ? control.characterLimit : 0
        property bool showBorder: control.hasOwnProperty("showBorder") ? control.showBorder : true

        Rectangle {
            id: underline
            color: background.hasError ? background.errorColor
                                    : control.activeFocus ? background.color
                                                          : Qt.rgba(1, 1, 1, 0.3)

            height: control.activeFocus ? 2 * calaosApp.density : 1 * 2 * calaosApp.density
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


        Label {
            id: fieldPlaceholder

            anchors.verticalCenter: parent.verticalCenter
            text: control.placeholderText
            font.pixelSize: 16 * calaosApp.density
            anchors.margins: -12  * calaosApp.density
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
                        font.pixelSize: 12 * calaosApp.density
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
                topMargin: 4 * calaosApp.density
            }

            Label {
                id: helperTextLabel
                visible: background.helperText && background.showBorder
                text: background.helperText
                font.pixelSize: 12 * calaosApp.density
                color: background.hasError ? background.errorColor
                                           : Qt.darker(Qt.rgba(1, 1, 1, 0.3))

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }

                property string helperText: control.hasOwnProperty("helperText")
                        ? control.helperText : ""
            }

            Label {
                id: charLimitLabel
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                visible: background.characterLimit && background.showBorder
                text: control.length + " / " + background.characterLimit
                font.pixelSize: 12 * calaosApp.density
                color: background.hasError ? background.errorColor : Qt.rgba(1, 1, 1, 0.3)
                horizontalAlignment: Text.AlignLeft

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }
    }

}

