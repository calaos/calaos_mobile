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
                             default_delegate

            onLoaded: {
                item.modelData = Qt.binding(function() { return model })
            }
        }
    }

    Component {
        id: default_delegate

        BorderImage {
            property variant modelData

            source: "qrc:/img/back_items_home.png"
            border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
            border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 10 * calaosApp.density
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
}

