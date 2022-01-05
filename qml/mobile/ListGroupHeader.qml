import QtQuick
import SharedComponents

Item {

    property alias title: titl.text

    height: titl.implicitHeight + 10 * calaosApp.density

    Text {
        id: titl
        color: "#3ab4d7"
        font { family: calaosFont.fontFamily; bold: true; pointSize: 12 }
        text: title
        elide: Text.ElideRight
        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            top: parent.top; topMargin: 4 * calaosApp.density
        }
    }
    Rectangle {
        color: "#424242"
        height: 2 * calaosApp.density
        anchors {
            left: parent.left; leftMargin: 12 * calaosApp.density
            right: parent.right; rightMargin: 12 * calaosApp.density
            bottom: parent.bottom
        }
    }
}
