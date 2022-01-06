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
        slider_hue.value = calaosApp.getColorHue(itemColor.rgbColor)
        slider_saturation.value = calaosApp.getColorSaturation(itemColor.rgbColor)
        slider_lumi.value = calaosApp.getColorLightness(itemColor.rgbColor)
    }

    property alias colorPreview: colorPrev.color

    function updatePreview() {
        console.log("hue:" + slider_hue.value/ 255.0 + " s:" + slider_saturation.value/ 100.0 + " l:" + slider_lumi.value/ 100.0)

        colorPreview = Qt.hsla(slider_hue.value / 255.0,
                               slider_saturation.value / 100.0,
                               slider_lumi.value / 100.0, 1)
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
                id: slider_hue
                Layout.fillWidth: true

                live: true
                from: 0; to: 255
                onValueChanged: updatePreview()

                property color currentColor: Qt.hsla(slider_hue.value / 255.0, 1, 0.5, 1)

                customBackground: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        rotation: 90
                        width: parent.height
                        height: parent.width
                        gradient: Gradient {
                            GradientStop { position: 0/6; color: "red" }
                            GradientStop { position: 1/6; color: "magenta" }
                            GradientStop { position: 2/6; color: "blue" }
                            GradientStop { position: 3/6; color: "cyan" }
                            GradientStop { position: 4/6; color: "lime" }
                            GradientStop { position: 5/6; color: "yellow" }
                            GradientStop { position: 6/6; color: "red" }
                        }
                    }
                }
            }

            CalaosSlider {
                id: slider_saturation
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
                            GradientStop { position: 1; color: "#6d6d6d" }
                            GradientStop { position: 0; color: slider_hue.currentColor }
                        }
                    }
                }
            }

            CalaosSlider {
                id: slider_lumi
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
                            GradientStop { position: 0; color: "white" }
                            GradientStop { position: 0.5; color: slider_hue.currentColor }
                            GradientStop { position: 1; color: "black" }
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
