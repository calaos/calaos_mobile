import QtQuick
import "."
import QtQuick.Layouts
import QuickFlux
import "../quickflux"

ItemBase {
    property variant modelData

    RowLayout {

        anchors {
            left: parent.left; leftMargin: Units.dp(8)
            right: parent.right; rightMargin: Units.dp(8)
            verticalCenter: parent.verticalCenter
        }

        Text {
            id: val
            color: "#ffda5a"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            clip: true
            text: modelData.stateInt + " " + modelData.unit
            elide: Text.ElideNone
        }

        ScrollingText {
            id: name
            color: "#3ab4d7"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            text: modelData.ioName
            clip: true
            Layout.fillWidth: true
        }

        SensorStatusIcon {
            sensor: modelData
        }

        IconItem {
            id: iconWarning

            source: calaosApp.getPictureSized("icon_warning")

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            visible: modelData.hasWarning

            MouseArea {
                anchors.fill: parent
                onClicked: AppActions.showNotificationMsg(qsTr("Warning"), qsTr("The value has not been updated for a long time"), qsTr("Close"))
            }
        }

        ItemButtonAction {
            id: btplus
            imageSource: "button_plus"

            onButtonClicked: {
                feedbackAnim()
                modelData.sendInc()
            }

            visible: modelData.rw
        }

        ItemButtonAction {
            id: btmin
            imageSource: "button_min"

            onButtonClicked:  {
                feedbackAnim()
                modelData.sendDec()
            }

            visible: modelData.rw
        }
    }
}
