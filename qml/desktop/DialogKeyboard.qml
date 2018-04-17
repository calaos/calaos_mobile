import QtQuick 2.3
import QtQuick.Controls 1.3 as QuickControls
import SharedComponents 1.0
import QtQuick.Layouts 1.2
import QtQuick.VirtualKeyboard 2.2
import QtQuick.VirtualKeyboard.Settings 2.2

Dialog {
    id: dlg

    width: rootWindow.width
    height: rootWindow.height

    hasActions: false
    contentMargins: 0
    flickableContent: false

    signal validClicked(string txt)

    property var __callback: null

    function openKeyboard(title, subtitle, initialText, cb) {
        lTitle.text = title
        lSubTitle.text = subtitle
        textInput.text = initialText

        if (cb) {
            __callback = cb
        } else {
            __callback = null
        }

        dlg.show()
    }

    ColumnLayout {

        spacing: 0

        anchors {
            left: parent.left
            right: parent.right
        }
        height: rootWindow.height

        Image {
            id: header
            source: "qrc:/img/module_header.png"
            Layout.fillWidth: true

            Text {
                id: lTitle
                anchors.centerIn: parent
                elide: Text.ElideMiddle
                font.pixelSize: Units.dp(22)
                font.family: calaosFont.fontFamilyLight
                font.weight: Font.ExtraLight
                color: "#e7e7e7"
                text: qsTr("Title")
            }
        }

        Image {
            source: "qrc:/img/module_header_shadow.png"
            Layout.fillWidth: true
            opacity: 0.6
        }

        Item { width: 1; Layout.preferredHeight: Units.dp(10) }

        Text {
            id: lSubTitle
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(16)
            font.family: calaosFont.fontFamilyLight
            horizontalAlignment: Text.AlignHCenter
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
            opacity: 0.8
            text: qsTr("Sub Title")
            Layout.fillWidth: true
        }

        Item { width: 1; Layout.preferredHeight: Units.dp(10) }

        Image {
            source: "qrc:/img/neon.png"
            Layout.fillWidth: true
        }

        FocusScope {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: textInput.implicitHeight
            focus: true

            TextInput {
                id: textInput

                font.pixelSize: Units.dp(20)
                font.family: calaosFont.fontFamilyLight
                font.weight: Font.ExtraLight
                color: "#3ab4d7"

                cursorVisible: activeFocus
                passwordCharacter: "\u2022"
                selectionColor: Qt.rgba(0.23, 0.71, 0.84, 0.30)
                selectedTextColor: color
                selectByMouse: true
                inputMethodHints: Qt.ImhPreferLowercase
                focus: true

                clip: true

                anchors {
                    fill: parent
                    margins: Units.dp(24)
                }
            }
        }

        Image {
            source: "qrc:/img/neon.png"
            Layout.fillWidth: true
        }

        InputPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
        }

        Image {
            id: footer
            source: "qrc:/img/module_footer.png"
            Layout.fillWidth: true

            RowLayout {
                anchors {
                    left: parent.left; leftMargin: Units.dp(20)
                    right: parent.right; rightMargin: Units.dp(20)
                    verticalCenter: parent.verticalCenter
                }

                spacing: Units.dp(5)

                Item { //spacer
                    height: 1; Layout.fillWidth: true
                }

                FooterButton {
                    label: qsTr("Clear current text")
                    icon: "qrc:/img/button_action_del.png"
                    Layout.minimumWidth: width
                    onBtClicked: textInput.text = ""
                }

                FooterButton {
                    label: qsTr("Valid")
                    icon: "qrc:/img/button_action_valid.png"
                    Layout.minimumWidth: width
                    onBtClicked: {
                        validClicked(textInput.text)
                        if (__callback) {
                            __callback(textInput.text)
                        }

                        dlg.close()
                    }
                }

                FooterButton {
                    label: qsTr("Quit")
                    icon: "qrc:/img/button_action_quit.png"
                    Layout.minimumWidth: width
                    onBtClicked: dlg.close()
                }
            }

        }
    }
}
