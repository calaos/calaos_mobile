import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1
import SharedComponents 1.0

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
                left: parent.left; leftMargin: 16 * calaosApp.density
                right: parent.right; rightMargin: 16 * calaosApp.density
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
                Layout.preferredHeight: 100 * calaosApp.density

                border.color: Qt.rgba(200, 200, 200, 0.1)
                border.width: 1 * calaosApp.density
                radius: 4 * calaosApp.density
                color: "transparent"

                Rectangle {
                    id: colorPrev
                    anchors {
                        fill: parent
                        margins: 4 * calaosApp.density
                    }
                }
            }

            //spacer
            Item { Layout.preferredHeight: 20 * calaosApp.density; Layout.fillWidth: true }

            CalaosItemBase {
                text: qsTr("Set color")
                Layout.preferredWidth: 200 * calaosApp.density
                Layout.preferredHeight: 40 * calaosApp.density
                Layout.alignment: Qt.AlignHCenter
                onButtonClicked: itemCallback(colorPreview)
            }
        }

        contentHeight: slidersLayout.height
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height + 20 * calaosApp.density
        flickableDirection: Flickable.VerticalFlick
    }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
        headerLabel: itemColor.ioName
        iconSource: calaosApp.getPictureSized("icon_light_on")
    }
}
