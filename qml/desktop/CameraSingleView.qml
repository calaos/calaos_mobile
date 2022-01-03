import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import Calaos 1.0

Item {
    property QtObject camModel

    Component.onCompleted: camModel.cameraVisible = true
    Component.onDestruction: camModel.cameraVisible = false

    Image {
        source: "qrc:/img/module_header_shadow.png"
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
        }
        opacity: 0.6
    }

    Image {
        source: "qrc:/img/module_footer_shadow.png"
        anchors {
            bottom: footer.top
            left: footer.left
            right: footer.right
        }
        opacity: 0.6
    }

    Image {
        id: cambg
        source: "qrc:/img/camera_big_border.png"
        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
        }

        Image {
            id: cameraPic
            source: camModel.url_single
            anchors {
                fill: parent
                leftMargin: Units.dp(19); rightMargin: Units.dp(19)
                topMargin: Units.dp(18); bottomMargin: Units.dp(18)
            }
        }  
    }

    CameraCommandItem {
        height: Units.dp(40)

        visible: camModel.hasPTZ

        anchors {
            top: cambg.top ; topMargin: Units.dp(31);
            right: parent.right
            left: cambg.right
        }
    }

    Image {
        id: header
        source: "qrc:/img/module_header.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Text {
            anchors.centerIn: parent
            elide: Text.ElideMiddle
            font.pixelSize: Units.dp(22)
            font.family: calaosFont.fontFamilyLight
            font.weight: Font.ExtraLight
            color: "#e7e7e7"
            text: camModel.name
        }
    }

    Image {
        id: footer
        source: "qrc:/img/module_footer.png"
        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }

        RowLayout {
            anchors {
                left: parent.left; leftMargin: Units.dp(20)
                right: parent.right; rightMargin: Units.dp(20)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(5)

            Item { //spacer
                height: 1; Layout.fillWidth: true
            }

            FooterButton {
                label: qsTr("Back")
                icon: "qrc:/img/button_action_back.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.handleBack()
            }

            FooterButton {
                label: qsTr("Quit")
                icon: "qrc:/img/button_action_quit.png"
                Layout.minimumWidth: width
                onBtClicked: rootWindow.goToDesktop()
            }
        }
    }
}
