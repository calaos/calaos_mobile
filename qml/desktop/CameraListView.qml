import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1

Item {

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
        source: "qrc:/img/neon.png"
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            right: parent.right
        }
    }

    Item {
        id: listview
        anchors {
            verticalCenter: parent.verticalCenter
            horizontalCenter: parent.horizontalCenter
            verticalCenterOffset: Units.dp(-80)
        }
        clip: true
        height: Units.dp(300)
        width: Units.dp(218) * 4 + 3 * Units.dp(10)

        ListView {
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: Units.dp(10)
            snapMode: ListView.SnapOneItem

            model: Math.ceil(cameraModel.cameraCount() / 4)
            delegate: Row {
                property int page: index + 1
                height: Units.dp(300)
                spacing: Units.dp(10)
                Repeater {
                    model: 4
                    CameraItem {
                        Component.onCompleted: {
                            var idx = page * modelData
                            if (idx < cameraModel.cameraCount()) {
                                camModel = Qt.binding(function() {
                                    return cameraModel.getItemModel(idx)
                                })
                                camConnected = true
                            } else {
                                camConnected = false
                            }
                        }
                    }
                }
            }
        }
    }

    RoundButton {
        next: false
        anchors {
            verticalCenter: parent.verticalCenter
            right: listview.left; rightMargin: Units.dp(-8)
        }
    }

    RoundButton {
        next: true
        anchors {
            verticalCenter: parent.verticalCenter
            left: listview.right; leftMargin: Units.dp(-8)
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
            text: qsTr("CCTV Camera")
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
                label: qsTr("Back to media")
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
