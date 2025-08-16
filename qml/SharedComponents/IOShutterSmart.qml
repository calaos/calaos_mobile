import QtQuick
import QtQuick.Layouts
import "."

ItemBase {
    property variant modelData

    height: 130 * calaosApp.density

    RowLayout {
        id: row

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
            verticalCenterOffset: Units.dp(-40)
        }

        ScrollingText {
            id: name
            color: !modelData.stateShutterBool?"#ffda5a":"#3ab4d7"
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
            id: btdown
            imageSource: "button_down2"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendDown()
            }
        }

        ItemButtonAction {
            id: btstop
            imageSource: "button_stop"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendStop()
            }
        }

        ItemButtonAction {
            id: btup
            imageSource: "button_up2"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendUp()
            }
        }
    }

    Text {
        id: stateTxt
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 11 }
        text: modelData.stateShutterTxt
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: windowBg.right; leftMargin: 8 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            top: row.bottom; topMargin: 18 * calaosApp.density
        }
    }

    Text {
        id: stateTxtAct
        color: "#e7e7e7"
        font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 11 }
        text: modelData.stateShutterTxtAction
        clip: true
        elide: Text.ElideMiddle
        anchors {
            left: windowBg.right; leftMargin: 8 * calaosApp.density
            right: parent.right; rightMargin: 8 * calaosApp.density
            top: stateTxt.bottom; topMargin: 8 * calaosApp.density
        }
    }

    property int shutterPos: modelData.stateShutterPos

    Item {
        anchors {
            horizontalCenterOffset: 7 * calaosApp.density
            centerIn: windowBg
        }
        clip: true
        height: shutter.height
        width: shutter.width
        Image {
            id: shutter
            source: calaosApp.getPictureSized("part_shutter2")
            anchors.centerIn: parent

            // shutterPos can be 0 <-> 100
            // picture should be -45 <-> 0
            anchors.verticalCenterOffset: Math.round(shutterPos * Units.dp(45) / 100) - Units.dp(45)
        }
    }

    Image {
        id: windowBg
        source: calaosApp.getPictureSized("part_shutter")
        anchors {
            left: parent.left; leftMargin: 14 * calaosApp.density
            top: row.bottom; topMargin: 8 * calaosApp.density
        }
    }

}
