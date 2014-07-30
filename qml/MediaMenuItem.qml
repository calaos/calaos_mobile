import QtQuick 2.0

Image {

    property Component icon
    property alias label: txt.text

    width: 198 * calaosApp.density
    height: 138 * calaosApp.density

    source: "qrc:/img/menu_media_item.png"

    Item {

        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 76 * calaosApp.density

        clip: true

        Loader {
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
            }
            sourceComponent: icon
        }
    }

    Image {
        source: "qrc:/img/menu_media_item_separator.png"
        width: parent.width
        height: parent.height
        anchors.fill: parent
    }

    Text {
        id: txt
        color: "#e7e7e7"
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        font { bold: true; pointSize: 14 }
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            topMargin: 76 * calaosApp.density
            bottom: parent.bottom
        }
        elide: Text.ElideMiddle
    }
}
