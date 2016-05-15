import QtQuick 2.0
import QtQuick.Controls 1.2
import QtQuick.Layouts 1.1

ItemBase {
    property variant modelData

    height: colorLayout.implicitHeight +
            40 * calaosApp.density //default height + sliders

    AnimatedIcon {
        id: icon

        countImage: 9
        imageFilenameOn: "icon_light_00%1"
        imageFilenameOff: "icon_light_off"

        anchors {
            left: parent.left; leftMargin: 8 * calaosApp.density
            verticalCenter: parent.top
            verticalCenterOffset: 22 * calaosApp.density
        }

        iconState: modelData.stateInt > 0
    }

    Text {
        color: icon.iconState?"#ffda5a":"#3ab4d7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
        text: modelData.ioName
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: icon.right; leftMargin: 8 * calaosApp.density
            right: btoff.left; rightMargin: 8 * calaosApp.density
            verticalCenter: icon.verticalCenter
        }

        Behavior on color { ColorAnimation { duration: 200 } }
    }

    ItemButtonAction {
        id: bton
        anchors {
            right: parent.right; rightMargin: 8 * calaosApp.density
            verticalCenter: icon.verticalCenter
        }
        imageSource: "button_light_on"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendTrue()
        }
    }

    ItemButtonAction {
        id: btoff
        anchors {
            right: bton.left; rightMargin: 8 * calaosApp.density
            verticalCenter: icon.verticalCenter
        }
        imageSource: "button_light_off"

        onButtonClicked: {
            feedbackAnim()
            modelData.sendFalse()
        }
    }

    ColumnLayout {
        id: colorLayout

        anchors {
            left: parent.left; leftMargin: 16 * calaosApp.density
            right: parent.right; rightMargin: 16 * calaosApp.density
            top: bton.bottom; bottomMargin: 8 * calaosApp.density
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
