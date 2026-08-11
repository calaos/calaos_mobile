import QtQuick
import Calaos
import QtQuick.Controls
import SharedComponents
import QtQuick.Layouts

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

        spacing: Units.dp(10)

        delegate: delegate

        model: audioModel

        Component {
            id: delegate

            Item {

                Component.onCompleted: {
                    modelData = Qt.binding(function() { return audioModel.getItemModel(model.index) })
                }
                property variant modelData

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -Units.dp(3)
                width: parent.width - Units.dp(16)
                height: Units.dp(200)

                BorderImage {
                    source: calaosApp.getPictureSized("back_items_home")
                    border.left: Units.dp(5); border.top: Units.dp(5)
                    border.right: Units.dp(5); border.bottom: Units.dp(5)

                    anchors.fill: parent
                }

                IconMusicPlayer {
                    id: iconcover
                    coverSource: modelData.cover
                    playing: modelData.status === Common.StatusPlay

                    anchors {
                        left: parent.left; leftMargin: Units.dp(8)
                        top: parent.top; topMargin: Units.dp(8)
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
                        left: iconcover.right; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        top: parent.top; topMargin: Units.dp(18)
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
                        left: iconcover.right; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        top: tname.bottom; topMargin: Units.dp(4)
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
                        left: iconcover.right; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        top: ttitle.bottom; topMargin: Units.dp(4)
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
                        left: iconcover.right; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        top: tartist.bottom; topMargin: Units.dp(4)
                    }
                }

                RowLayout {
                    anchors {
                        left: parent.left; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        bottom: row.top; bottomMargin: Units.dp(8)
                    }

                    Text {
                        id: tvol
                        color: "#3ab4d7"
                        font { family: calaosFont.fontFamily; bold: false; pointSize: 11 }
                        text: qsTr("Volume:")
                        horizontalAlignment: Text.AlignLeft
                        clip: true
                        elide: Text.ElideRight
                    }

                    CalaosSlider {
                        id: volumeSlider

                        value: modelData.volume
                        live: false
                        from: 0; to: 100
                        onValueChanged: modelData.sendVolume(value)
                    }
                }

                Row {

                    id: row

                    spacing: Units.dp(4)

                    anchors {
                        right: parent.right; rightMargin: Units.dp(8)
                        bottom: parent.bottom; bottomMargin: Units.dp(8)
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
