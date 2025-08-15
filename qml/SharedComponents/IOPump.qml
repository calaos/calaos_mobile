import QtQuick
import QtQuick.Layouts
import "."

ItemBase {
    property variant modelData

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        IconItem {
            id: iconBg
            source: calaosApp.getPictureSized("icon_pump_bg")
            Layout.preferredHeight: height
            Layout.preferredWidth: width

            AnimatedIcon {
                id: icon

                countImage: 9
                imageFilenameOn: "icon_pump_00%1"
                imageFilenameOff: "icon_pump_off"

                iconState: modelData.stateBool

                onAnimationFinished: {
                    if (iconState)
                        animRot.running = true
                    else
                        animRot.running = false
                }

                RotationAnimation {
                    id: animRot
                    target: icon;
                    from: 0;
                    to: 360;
                    duration: 1000
                    running: false
                    loops: Animation.Infinite
                }
            }
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
            id: btoff
            iconSource: "qrc:/img/ic_outlet_off.svg"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendFalse()
            }
        }

        ItemButtonAction {
            id: bton
            iconSource: "qrc:/img/ic_outlet_on.svg"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendTrue()
            }
        }
    }
}
