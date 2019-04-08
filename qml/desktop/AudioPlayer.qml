import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import Calaos 1.0
import QuickFlux 1.0
import "../quickflux"
import "Utils.js" as Utils

Item {
    property variant playerModel
    property bool hasModel: playerModel !== undefined

    width: Units.dp(290)
    height: Units.dp(502)

    opacity: hasModel? 1.0: 0.35

    Item {
        width: Units.dp(229)
        height: Units.dp(502)
        anchors.centerIn: parent

        ColumnLayout {
            id: colBackCover
            anchors {
                left: parent.left; leftMargin: Units.dp(42)
                top: parent.top
                right: parent.right
            }

            RowLayout {
                Image {
                    source: "qrc:/img/icon_player.png"
                    Layout.alignment: Qt.AlignVCenter
                }
                Text {
                    id: labelName
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
                    text: hasModel? playerModel.name: qsTr("Aucune zone")
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideMiddle
                    color: "white"
                }
            }

            Item {
                Layout.preferredHeight: Units.dp(170)
                Layout.preferredWidth: Units.dp(188)

                Image {
                    source: "qrc:/img/desktop/player_shadow.png"
                    anchors.centerIn: parent
                }

                Image {
                    source: "qrc:/img/desktop/player_cover_back.png"
                    anchors.centerIn: parent
                }

                Image {
                    source: "qrc:/img/desktop/player_cover_front.png"
                    anchors.centerIn: parent
                }
            }

            Item { Layout.preferredHeight: Units.dp(13); width: 1 }

            RowLayout {
                Layout.fillWidth: true
                spacing: Units.dp(11)

                ItemButtonAction {
                    id: btprevious
                    imageSource: "button_previous"
                    onButtonClicked: playerModel.sendPrevious()
                }

                ItemButtonAction {
                    id: btplay
                    iconSource: !hasModel? "qrc:/img/ic_play.svg": playerModel.status === Common.StatusPlay? "qrc:/img/ic_pause.svg": "qrc:/img/ic_play.svg"
                    onButtonClicked: playerModel.status === Common.StatusPlay?
                                         playerModel.sendPause(): playerModel.sendPlay()
                }

                ItemButtonAction {
                    id: btstop
                    imageSource: "button_stop"
                    onButtonClicked: playerModel.sendStop()
                }

                ItemButtonAction {
                    id: btnext
                    imageSource: "button_next"
                    onButtonClicked: playerModel.sendNext()
                }
            }

            Item { Layout.preferredHeight: Units.dp(12); width: 1 }

            RowLayout {
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 8 }
                    text: qsTr("Elapsed:")
                    color: "#848484"
                    Layout.fillWidth: true
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 8 }
                    text: playerModel.elapsed === ""? "N/A": Utils.time2string_digit(playerModel.elapsed)
                    color: "#848484"
                }
            }

            Rectangle {
                Layout.preferredHeight: Units.dp(2)
                Layout.fillWidth: true
                color: Qt.rgba(255, 255, 255, 0.2)
                Rectangle {
                    height: parent.height
                    width: parent.width * (playerModel.elapsed / playerModel.duration)
                    color: "#3AB4D7"
                }
            }

            Item { Layout.preferredHeight: Units.dp(34); width: 1 }

            Item {
                Layout.preferredHeight: Units.dp(16)
                Layout.preferredWidth: Units.dp(218)

                Item {
                    id: cdSmall
                    width: Units.dp(91)
                    clip: true

                    visible: hasModel

                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        top: parent.top; topMargin: Units.dp(-5)
                        bottom: parent.top; bottomMargin: Units.dp(-15)
                    }

                    state: playerModel.status === Common.StatusPlay? "playing": "stopped"

                    Image {
                        source: "qrc:/img/cd_01.png"

                        RotationAnimator on rotation {
                            running: playerModel.status === Common.StatusPlay
                            loops: Animation.Infinite
                            duration: 2000
                            from: 0 ; to: 360
                        }
                    }

                    states: [
                        State { name: "playing"; PropertyChanges { target: cdSmall; anchors.topMargin: -5 } },
                        State { name: "stopped"; PropertyChanges { target: cdSmall; anchors.topMargin: -30 } }
                    ]

                    transitions: Transition {
                        NumberAnimation { target: cdSmall; property: "anchors.topMargin"; easing.type: Easing.OutCubic; duration: 400 }
                    }
                }

                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: "qrc:/img/desktop/player_line_decoration_top.png"
                }

                Image {
                    source: "qrc:/img/desktop/player_line_decoration_cd.png"
                    visible: hasModel
                }
            }

            Item { Layout.preferredHeight: Units.dp(10); width: 1 }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 0

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: qsTr("Artist:")
                    color: "#848484"
                    Layout.alignment: Qt.AlignRight
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: playerModel.artist === ""? "N/A": playerModel.artist
                    color: playerModel.status === Common.StatusPlay? "#3AB4D7": "#848484"
                }

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: qsTr("Album:")
                    color: "#848484"
                    Layout.alignment: Qt.AlignRight
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: playerModel.album === ""? "N/A": playerModel.album
                    color: playerModel.status === Common.StatusPlay? "#3AB4D7": "#848484"
                }

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: qsTr("Track:")
                    color: "#848484"
                    Layout.alignment: Qt.AlignRight
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: playerModel.title === ""? "N/A": playerModel.title
                    color: playerModel.status === Common.StatusPlay? "#3AB4D7": "#848484"
                }

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: qsTr("Genre:")
                    color: "#848484"
                    Layout.alignment: Qt.AlignRight
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: playerModel.genre === ""? "N/A": playerModel.genre
                    color: playerModel.status === Common.StatusPlay? "#3AB4D7": "#848484"
                }

                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: qsTr("Duration:")
                    color: "#848484"
                    Layout.alignment: Qt.AlignRight
                }
                Text {
                    font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 10 }
                    text: playerModel.duration === ""? "N/A": Utils.time2string_digit(playerModel.duration)
                    color: playerModel.status === Common.StatusPlay? "#3AB4D7": "#848484"
                }
            }
        }

        Text {
            id: labelVol
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 8 }
            text: "%1%".arg(playerModel.volume)
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
            color: "#848484"
            anchors {
                left: sliderVolume.left; right: sliderVolume.right
                bottom: sliderVolume.top; bottomMargin: Units.dp(5)
            }
        }

        CalaosSlider {
            id: sliderVolume
            orientation: Qt.Vertical
            height: Units.dp(170)
            anchors {
                left: parent.left; leftMargin: Units.dp(0)
                top: parent.top; topMargin: Units.dp(28)
            }
            updateValueWhileDragging: false
            maximumValue: 100
            value: playerModel.volume
            onValueChanged: playerModel.sendVolume(value)
        }
    }

    MouseArea {
        //blocks events when no model
        anchors.fill: parent
        enabled: !hasModel
    }
}
