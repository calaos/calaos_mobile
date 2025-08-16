import QtQuick
import QtQuick.Layouts
import "."

ItemBase {
    property variant modelData

    height: colorLayout.implicitHeight +
            40 * calaosApp.density //default height + sliders

    RowLayout {
        id: row

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.top
            verticalCenterOffset: Units.dp(22)
        }

        AnimatedIcon {
            id: icon

            countImage: 9
            imageFilenameOn: "icon_light_00%1"
            imageFilenameOff: "icon_light_off"

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            iconState: modelData.rgbColor.r > 0 ||
                       modelData.rgbColor.g > 0 ||
                       modelData.rgbColor.b > 0
        }

        ScrollingText {
            color: icon.iconState?"#ffda5a":"#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.ioName
            clip: true
            Layout.fillWidth: true

            Behavior on color { ColorAnimation { duration: 200 } }
        }

        SensorStatusIcon {
            sensor: modelData
        }

        ItemButtonAction {
            id: bton
            imageSource: "button_light_on"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }
        }

        ItemButtonAction {
            id: btoff
            imageSource: "button_light_off"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }
        }
    }

    ColumnLayout {
        id: colorLayout

        anchors {
            left: parent.left; leftMargin: 16 * calaosApp.density
            right: parent.right; rightMargin: 16 * calaosApp.density
            top: row.bottom; bottomMargin: 8 * calaosApp.density
        }

        MouseArea {
            id: currColor

            Layout.preferredWidth: 130 * calaosApp.density
            Layout.minimumHeight: 50 * calaosApp.density
            Layout.alignment: Qt.AlignRight

            Rectangle {
                anchors {
                    fill: parent
                    margins: 10 * calaosApp.density
                    rightMargin: 0
                }
                border.color: Qt.rgba(200, 200, 200, 0.1)
                border.width: 1 * calaosApp.density
                radius: 4 * calaosApp.density
                color: "transparent"

                Rectangle {
                    anchors {
                        fill: parent
                        margins: 4 * calaosApp.density
                    }
                    color: modelData.rgbColor
                }
            }

            onClicked: openColorPicker(modelData, function(c) {
                feedbackAnim()
                modelData.sendColor(c)
            })
        }
    }
}
