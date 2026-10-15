import QtQuick
import Calaos
import QtQuick.Controls
import SharedComponents

Item {

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    ListView {
        id: lst

        width: parent.width
        height: parent.height - header.height
        y: header.height

        spacing: Units.dp(10)

        delegate: delegate

        model: cameraModel

        Component {
            id: delegate

            Item {

                Component.onCompleted: {
                    modelData = Qt.binding(function() { return cameraModel.getItemModel(model.index) })
                }
                property variant modelData

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -Units.dp(3)
                width: parent.width - Units.dp(16)
                height: Units.dp(130)

                BorderImage {
                    source: calaosApp.getPictureSized("back_items_home")
                    border.left: Units.dp(5); border.top: Units.dp(5)
                    border.right: Units.dp(5); border.bottom: Units.dp(5)

                    anchors.fill: parent
                }

                BorderImage {
                    id: backcam

                    source: calaosApp.getPictureSized("camera_back")
                    border.left: Units.dp(13); border.top: Units.dp(13)
                    border.right: Units.dp(13); border.bottom: Units.dp(13)
                    height: Units.dp(113)
                    width: Units.dp(144)

                    anchors {
                        left: parent.left; leftMargin: Units.dp(8)
                        top: parent.top; topMargin: Units.dp(8)
                    }

                    Image {
                        id: cameraPic

                        source: modelData.url_single

                        anchors {
                            left: parent.left; leftMargin: Units.dp(13)
                            top: parent.top; topMargin: Units.dp(13)
                            right: parent.right; rightMargin: Units.dp(13)
                            bottom: parent.bottom; bottomMargin: Units.dp(13)
                        }
                    }
                }

                Text {
                    id: tname
                    color: "#3ab4d7"
                    font { family: calaosFont.fontFamily; bold: false; pointSize: 13 }
                    text: modelData.name
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: backcam.right; leftMargin: Units.dp(8)
                        right: parent.right; rightMargin: Units.dp(8)
                        top: parent.top; topMargin: Units.dp(18)
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        //set only camera visible for current camera
                        isSingleCameraView = true
                        cameraModel.cameraVisible = false
                        modelData.cameraVisible = true
                        currentCameraModel = modelData
                        stackView.push(cameraSingleView)
                    }
                }
            }
        }
    }

    ScrollBar { listObject: lst }

    ViewHeader {
        id: header
        headerLabel: qsTr("Camera")
        iconSource: calaosApp.getPictureSized("icon_media")
    }
}
