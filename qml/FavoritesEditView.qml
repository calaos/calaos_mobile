import QtQuick 2.0
import Calaos 1.0;

Item {
    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: listViewFav
        model: favoritesModel

        y: header.height
        width: parent.width
        height: parent.height - header.height

        spacing: 5 * calaosApp.density

        delegate: default_delegate
    }

    Text {
        color: "#3ab4d7"
        font { bold: false; pointSize: 12 }
        anchors.centerIn: listViewFav
        text: qsTr("No favorites yet.<br>Go to <b>Config</b> to add some.")
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        visible: listViewFav.count == 0
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
    }

    ScrollBar { listObject: listViewFav }

    ViewHeader {
        id: header
        headerLabel: qsTr("Edit favorites list")
        iconSource: calaosApp.getPictureSized("fav")
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: calaosApp.getPictureSized("back_items_home")
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            width: parent.width - 10 * calaosApp.density
            height: 40 * calaosApp.density

            Text {
                color: "#3ab4d7"
                font { bold: false; pointSize: 12 }
                text: favName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: parent.left; leftMargin: 8 * calaosApp.density
                    right: btadd.left; rightMargin: 8 * calaosApp.density
                    verticalCenter: parent.verticalCenter
                }
            }

            ItemButtonAction {
                id: btadd
                anchors {
                    right: parent.right; rightMargin: 8 * calaosApp.density
                    verticalCenter: parent.verticalCenter
                }
                imageSource: "button_min"

                onButtonClicked: {
                    popup.opacity = 1
                    tmr.restart()
                    calaosApp.delItemFavorite(index)
                }
            }
        }

    }

    Timer {
        id: tmr
        interval: 1000
        repeat: false
        running: false
        onTriggered: popup.opacity = 0
    }

    Rectangle {
        id: popup
        color: "black"
        anchors {
            left: parent.left; right: parent.right
            verticalCenter: parent.verticalCenter
        }
        height: txt.implicitHeight + 36 * calaosApp.density

        visible: opacity > 0
        opacity: 0
        Behavior on opacity { NumberAnimation {} }

        Text {
            id: txt
            color: "#e7e7e7"
            font { bold: false; pointSize: 12 }
            text: "Item removed from favorites"
            clip: true
            elide: Text.ElideMiddle
            anchors.centerIn: parent
        }
    }
}
