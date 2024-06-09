import QtQuick
import SharedComponents
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings

Dialog {
    id: dlg

    width: rootWindow.width
    height: rootWindow.height

    hasActions: false
    contentMargins: 0
    flickableContent: false

    signal validClicked(string txt)

    property var __callback: null
    property bool multiline: false
    property string currentText

    function openKeyboard(title, subtitle, initialText, inputEchoMode, multiline, cb) {
        lTitle.text = title
        lSubTitle.text = subtitle
        dlg.multiline = multiline
        currentText = initialText

        if (multiline) {
            textInputMulti.forceActiveFocus()
        } else {
            textInput.forceActiveFocus()
        }

        if (cb) {
            __callback = cb
        } else {
            __callback = null
        }

        textInput.echoMode = inputEchoMode
        //textInputMulti.echoMode = inputEchoMode

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
            clip: true

            TextInput {
                id: textInput

                font.pixelSize: Units.dp(20)
                font.family: calaosFont.fontFamilyLight
                font.weight: Font.ExtraLight
                color: "#3ab4d7"

                text: currentText
                onTextChanged: {
                    currentText = text
                    textInput.text = Qt.binding(function() { return currentText })
                }

                visible: !dlg.multiline
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

            Flickable {
                id: flickText

                anchors {
                    fill: parent
                    leftMargin: Units.dp(24); rightMargin: Units.dp(24)
                    topMargin: flickText.visibleArea.heightRatio == 1? Units.dp(24):0
                }

                visible: dlg.multiline
                contentHeight: textInputMulti.implicitHeight
                contentWidth: textInputMulti.width

                function ensureVisible(r) {
                    if (contentY >= r.y)
                        contentY = r.y;
                    else if (contentY + height <= r.y + r.height)
                        contentY = r.y + r.height - height;
                }

                TextEdit {
                    id: textInputMulti

                    font.pixelSize: Units.dp(20)
                    font.family: calaosFont.fontFamilyLight
                    font.weight: Font.ExtraLight
                    color: "#3ab4d7"

                    wrapMode: TextEdit.Wrap
                    visible: dlg.multiline

                    text: currentText
                    onTextChanged: {
                        currentText = text
                        textInputMulti.text = Qt.binding(function() { return currentText })
                    }

                    cursorVisible: activeFocus
                    selectionColor: Qt.rgba(0.23, 0.71, 0.84, 0.30)
                    selectedTextColor: color
                    selectByMouse: true
                    inputMethodHints: Qt.ImhPreferLowercase
                    focus: true

                    clip: true

                    width: flickText.width
                    height: implicitHeight

                    onCursorRectangleChanged: flickText.ensureVisible(cursorRectangle)
                }
            }

            ScrollBar { listObject: flickText }
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
                    onBtClicked: {
                        currentText = ""
                        textInput.text = Qt.binding(function() { return currentText })
                        textInputMulti.text = Qt.binding(function() { return currentText })
                    }
                }

                FooterButton {
                    label: qsTr("Valid")
                    icon: "qrc:/img/button_action_valid.png"
                    Layout.minimumWidth: width
                    onBtClicked: {
                        validClicked(currentText)
                        if (__callback) {
                            __callback(currentText)
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
