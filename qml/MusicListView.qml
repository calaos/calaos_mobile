import QtQuick 2.2
import "calaos.js" as Calaos;
import QtQuick.Controls 1.2

Item {

    property alias model: lst.model
    property alias visibleArea: lst.visibleArea

    ListView {
        id: lst

        width: parent.width
        height: parent.height - header.height
        y: header.height

        spacing: 10 * calaosApp.density

        delegate: delegate

        Component {
            id: delegate

            Item {

                Component.onCompleted: {
                    console.log("component loaded")
                    modelData = Qt.binding(function() { return audioModel.getItemModel(model.index) })
                }
                property variant modelData

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -3 * calaosApp.density
                width: parent.width - 16 * calaosApp.density
                height: 200 * calaosApp.density

                BorderImage {
                    source: calaosApp.getPictureSized("back_items_home")
                    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
                    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

                    anchors.fill: parent
                }

                IconMusicPlayer {
                    id: cover
                    coverSource: audioCoverSource

                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 8 * calaosApp.density
                        bottom: parent.bottom; bottomMargin: 8 * calaosApp.density
                    }
                }

                Text {
                    id: tname
                    color: "#3ab4d7"
                    font { bold: false; pointSize: 13 }
                    text: audioName
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: cover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 18 * calaosApp.density
                    }
                }

                Text {
                    id: ttitle
                    color: "#e7e7e7"
                    font { bold: false; pointSize: 11 }
                    text: audioTitle
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: cover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: tname.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: tartist
                    color: "#e7e7e7"
                    font { bold: false; pointSize: 11 }
                    text: audioArtist
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: cover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: ttitle.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: talbum
                    color: "#e7e7e7"
                    font { bold: false; pointSize: 11 }
                    text: audioAlbum
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: cover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: tartist.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: tvol
                    color: "#3ab4d7"
                    font { bold: false; pointSize: 11 }
                    text: qsTr("Volume:")
                    horizontalAlignment: Text.AlignLeft
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: talbum.bottom; topMargin: 8 * calaosApp.density
                    }
                }

                Slider {
                    id: volumeSlider
                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: tvol.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Row {

                    spacing: 4 * calaosApp.density

                    anchors {
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        bottom: parent.bottom; bottomMargin: 8 * calaosApp.density
                    }

                    ItemButtonAction {
                        imageSource: "button_previous"

                        onButtonClicked: modelData.sendPrevious()
                    }

                    ItemButtonAction {
                        imageSource: "button_play"

                        onButtonClicked: modelData.sendPlay()
                    }

                    ItemButtonAction {
                        imageSource: "button_stop"

                        onButtonClicked: modelData.sendStop()
                    }

                    ItemButtonAction {
                        imageSource: "button_next"

                        onButtonClicked: modelData.sendNext()
                    }
                }
            }

        }
    }

    ViewHeader {
        id: header
        headerLabel: qsTr("Music")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
