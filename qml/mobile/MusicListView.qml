import QtQuick 2.2
import Calaos 1.0
import QtQuick.Controls 1.2
import SharedComponents 1.0

Item {

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: lst

        width: parent.width
        height: parent.height - header.height
        y: header.height

        spacing: 10 * calaosApp.density

        delegate: delegate

        model: audioModel

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
                    id: iconcover
                    coverSource: cover
                    playing: modelData.status === Common.StatusPlay

                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 8 * calaosApp.density
                    }
                }

                Text {
                    id: tname
                    color: "#3ab4d7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 13 }
                    text: modelData.name
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: iconcover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 18 * calaosApp.density
                    }
                }

                Text {
                    id: ttitle
                    color: "#e7e7e7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 11 }
                    text: modelData.title
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: iconcover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: tname.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: tartist
                    color: "#e7e7e7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 11 }
                    text: modelData.artist
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: iconcover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: ttitle.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: talbum
                    color: "#e7e7e7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 11 }
                    text: modelData.album
                    horizontalAlignment: Text.AlignRight
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: iconcover.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: tartist.bottom; topMargin: 4 * calaosApp.density
                    }
                }

                Text {
                    id: tvol
                    color: "#3ab4d7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 11 }
                    text: qsTr("Volume:")
                    horizontalAlignment: Text.AlignLeft
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        bottom: volumeSlider.top; bottomMargin: 4 * calaosApp.density
                    }
                }

                Slider {
                    id: volumeSlider
                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        bottom: row.top; bottomMargin: 8 * calaosApp.density
                    }
                    value: modelData.volume
                    updateValueWhileDragging: false
                    maximumValue: 100
                    onValueChanged: {
                        modelData.sendVolume(value)
                        value = Qt.binding(function() { return modelData.audioVolume })
                    }
                }

                Row {

                    id: row

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

    ScrollBar { listObject: lst }

    ViewHeader {
        id: header
        headerLabel: qsTr("Music")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
