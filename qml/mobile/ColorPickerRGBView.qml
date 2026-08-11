import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SharedComponents

Item {

    property QtObject itemColor
    property var itemCallback

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    onItemColorChanged: {
        colorPreview = itemColor.rgbColor
        slider_red.value = itemColor.rgbColor.r * 255
        slider_green.value = itemColor.rgbColor.g * 255
        slider_blue.value = itemColor.rgbColor.b * 255
        console.log("update sliders: r:" + slider_red.value + " g:" + slider_green.value + " b:" + slider_blue.value)
    }

    property alias colorPreview: colorPrev.color

    function updatePreview() {

        console.log("r:" + slider_red.value + " g:" + slider_green.value + " b:" + slider_blue.value)
        colorPreview = Qt.rgba(slider_red.value / 255.0, slider_green.value / 255.0, slider_blue.value / 255.0, 1)
    }

    Flickable {
        id: listFlick

        ColumnLayout {
            id: slidersLayout

            anchors {
                left: parent.left; leftMargin: Units.dp(16)
                right: parent.right; rightMargin: Units.dp(16)
            }

            CalaosSlider {
                id: slider_red
                Layout.fillWidth: true

                live: true
                from: 0; to: 255
                onValueChanged: updatePreview()

                property color currentColor: Qt.hsla(slider_red.value / 255.0, 1, 0.5, 1)

                customBackground: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        rotation: 90
                        width: parent.height
                        height: parent.width
                        gradient: Gradient {
                            GradientStop { position: 0; color: "red" }
                            GradientStop { position: 1; color: "black" }
                        }
                    }
                }
            }

            CalaosSlider {
                id: slider_green
                Layout.fillWidth: true

                live: true
                from: 0; to: 255
                onValueChanged: updatePreview()

                customBackground: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        rotation: 90
                        width: parent.height
                        height: parent.width
                        gradient: Gradient {
                            GradientStop { position: 0; color: "green" }
                            GradientStop { position: 1; color: "#6d6d6d" }
                        }
                    }
                }
            }

            CalaosSlider {
                id: slider_blue
                Layout.fillWidth: true

                live: true
                from: 0; to: 255
                onValueChanged: updatePreview()

                customBackground: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        rotation: 90
                        width: parent.height
                        height: parent.width
                        gradient: Gradient {
                            GradientStop { position: 0; color: "blue" }
                            GradientStop { position: 1; color: "#6d6d6d" }
                        }
                    }
                }
            }

            Rectangle {

                Layout.fillWidth: true
                Layout.preferredHeight: Units.dp(100)

                border.color: Qt.rgba(200, 200, 200, 0.1)
                border.width: Units.dp(1)
                radius: Units.dp(4)
                color: "transparent"

                Rectangle {
                    id: colorPrev
                    anchors {
                        fill: parent
                        margins: Units.dp(4)
                    }
                }
            }

            //spacer
            Item { Layout.preferredHeight: Units.dp(20); Layout.fillWidth: true }

            CalaosItemBase {
                text: qsTr("Set color")
                Layout.preferredWidth: Units.dp(200)
                Layout.preferredHeight: Units.dp(40)
                Layout.alignment: Qt.AlignHCenter
                onButtonClicked: itemCallback(colorPreview)
            }
        }

        contentHeight: slidersLayout.height
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height + Units.dp(20)
        flickableDirection: Flickable.VerticalFlick
    }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
        headerLabel: itemColor.ioName
        iconSource: calaosApp.getPictureSized("icon_light_on")
    }
}
