import QtQuick 2.0
import QtQuick.Controls 1.2

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: listFlick

        Column {
            id: content
            spacing: 6

            ListGroupHeader { width: listFlick.width; title: qsTr("Connection:") }
            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                Button {
                    text: qsTr("Logout")
                    width: 200 * calaosApp.density
                    height: parent.height
                    style: StyleButtonDefault { }
                    onClicked: calaosApp.logout()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                Button {
                    text: qsTr("Reset all data")
                    width: 200 * calaosApp.density
                    height: parent.height
                    style: StyleButtonDefault { }
                    onClicked: calaosApp.resetAllData()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Favorites:") }

        }

        contentHeight: content.height
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height
        flickableDirection: Flickable.VerticalFlick
    }

    ScrollBar {
        width: 10; height: listFlick.height
        anchors.right: parent.right
        opacity: 1
        orientation: Qt.Vertical
        wantBackground: false
        position: listFlick.visibleArea.yPosition
        pageSize: listFlick.visibleArea.heightRatio
    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Settings")
        iconSource: calaosApp.getPictureSized("icon_config")
    }
}
