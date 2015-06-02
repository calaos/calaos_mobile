import QtQuick 2.2
import Calaos 1.0
import QtQuick.Controls 1.2

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

        spacing: 10 * calaosApp.density

        delegate: delegate

        model: cameraModel

        Component {
            id: delegate

            Item {

                Component.onCompleted: {
                    console.log("component loaded")
                    modelData = Qt.binding(function() { return cameraModel.getItemModel(model.index) })
                }
                property variant modelData

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -3 * calaosApp.density
                width: parent.width - 16 * calaosApp.density
                height: 130 * calaosApp.density

                BorderImage {
                    source: calaosApp.getPictureSized("back_items_home")
                    border.left: 5 * calaosApp.density; border.top: 5 * calaosApp.density
                    border.right: 5 * calaosApp.density; border.bottom: 5 * calaosApp.density

                    anchors.fill: parent
                }

                BorderImage {
                    id: backcam

                    source: calaosApp.getPictureSized("camera_back")
                    border.left: 13 * calaosApp.density; border.top: 13 * calaosApp.density
                    border.right: 13 * calaosApp.density; border.bottom: 13 * calaosApp.density
                    height: 113 * calaosApp.density
                    width: 144 * calaosApp.density

                    anchors {
                        left: parent.left; leftMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 8 * calaosApp.density
                    }

                    Image {
                        id: cameraPic

                        source: modelData.url_single

                        anchors {
                            left: parent.left; leftMargin: 13 * calaosApp.density
                            top: parent.top; topMargin: 13 * calaosApp.density
                            right: parent.right; rightMargin: 13 * calaosApp.density
                            bottom: parent.bottom; bottomMargin: 13 * calaosApp.density
                        }
                    }
                }

                Text {
                    id: tname
                    color: "#3ab4d7"
                    font { bold: false; pointSize: 13 }
                    text: modelData.name
                    clip: true
                    elide: Text.ElideRight
                    anchors {
                        left: backcam.right; leftMargin: 8 * calaosApp.density
                        right: parent.right; rightMargin: 8 * calaosApp.density
                        top: parent.top; topMargin: 18 * calaosApp.density
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
