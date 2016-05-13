import QtQuick 2.4
import QtQuick.Controls 1.4 as Controls
import QtGraphicalEffects 1.0
import SharedComponents 1.0

Controls.TextField {

    property color color: "#3AB4D7"
    property color errorColor: "#3AB4D7"
    property string helperText
    property bool floatingLabel: false
    property bool hasError: characterLimit && length > characterLimit
    property int characterLimit
    property bool showBorder: true

    property bool showClearButton: true
    signal clearButtonClicked()

    style: TextFieldStyle { }

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
            onClicked: clearButtonClicked()
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
