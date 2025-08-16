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

        IconItem {
            id: icon

            Layout.preferredHeight: height
            Layout.preferredWidth: width

            source: modelData.ioStyle === "temperature"? calaosApp.getPictureSized("icon_temp"):
                        calaosApp.getPictureSized("icon_%1".arg(modelData.ioStyle))
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
            visible: modelData.hasWarning
            Layout.preferredHeight: height
            Layout.preferredWidth: width

            MouseArea {
                anchors.fill: parent
                onClicked: AppActions.showNotificationMsg(qsTr("Warning"), qsTr("The value has not been updated for a long time"), qsTr("Close"))
            }
        }

        Text {
            id: temp
            color: "#ffda5a"
            font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pointSize: 12 }
            clip: true
            text: modelData.stateInt + " " + modelData.unit
            elide: Text.ElideNone
        }
    }
}
