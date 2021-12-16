import QtQuick
import SharedComponents
import QtQuick.Layouts

Dialog {
    id: dlg

    property var __callback: null
    property QtObject __item

    function openWithIO(item, cb) {
        __callback = cb
        __item = item
        show()
    }

    title: qsTr("Choose a color")
    text: qsTr("Change the color of the light")
    hasActions: true
    positiveButtonText: qsTr("Change color")
    negativeButtonText: qsTr("Cancel")

    onAccepted: {
        __callback(colorPreview)
    }

    on__ItemChanged: {
        colorPreview = __item.rgbColor
        slider_red.value = __item.rgbColor.r * 255
        slider_green.value = __item.rgbColor.g * 255
        slider_blue.value = __item.rgbColor.b * 255
        console.log("update sliders: r:" + slider_red.value + " g:" + slider_green.value + " b:" + slider_blue.value)
    }

    property alias colorPreview: colorPrev.color

    function updatePreview() {
        console.log("r:" + slider_red.value + " g:" + slider_green.value + " b:" + slider_blue.value)
        colorPreview = Qt.rgba(slider_red.value / 255.0, slider_green.value / 255.0, slider_blue.value / 255.0, 1)
    }

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
    }
}
