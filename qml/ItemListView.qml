import QtQuick 2.2
import "calaos.js" as Calaos;
import Calaos 1.0

ListView {
    anchors.fill: parent

    spacing: 10 * calaosApp.density

    delegate: delegate

    Component {
        id: delegate

        Loader {
            z: index
            sourceComponent: model.ioType === Common.Light? light:
                             model.ioType === Common.Temp? temp:
                             model.ioType === Common.VarInt? var_int:
                             model.ioType === Common.AnalogIn? var_int:
                             model.ioType === Common.AnalogOut? var_int:
                             model.ioType === Common.VarBool? var_bool:
                             model.ioType === Common.VarString? var_string:
                             model.ioType === Common.Scenario? scenario:
                             model.ioType === Common.Shutter? shutter:
                             default_delegate

            onLoaded: {
                item.modelData = Qt.binding(function() { return roomModel.getItemModel(model.index) })
            }

            anchors.horizontalCenter: parent.horizontalCenter
            anchors.horizontalCenterOffset: -3 * calaosApp.density
            width: parent.width - 15 * calaosApp.density
        }
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: "qrc:/img/back_items_home.png"
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            width: parent.width
            height: 40 * calaosApp.density

            Text {
                color: "#3ab4d7"
                font { bold: false; pointSize: 12 }
                text: modelData.ioName
                clip: true
                elide: Text.ElideMiddle
                anchors {
                    left: parent.left; leftMargin: 8
                    right: parent.right; rightMargin: 8
                    verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Component { id: light; IOLight {} }
    Component { id: temp; IOTemp {} }
    Component { id: var_int; IOVarInt {} }
    Component { id: var_bool; IOVarBool {} }
    Component { id: var_string; IOVarString {} }
    Component { id: scenario; IOScenario {} }
    Component { id: shutter; IOShutter {} }
}

