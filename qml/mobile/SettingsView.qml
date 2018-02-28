import QtQuick 2.0
import QtQuick.Controls 1.2
import SharedComponents 1.0
import QtQuick.Controls 1.3 as QuickControls
import "../quickflux"

Item {

    signal favoriteAddClicked()
    signal favoriteEditClicked()

    Image {
        source: calaosApp.getPictureSized(isLandscape?
                                              "background_landscape":
                                              "background")
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
    }

    Flickable {
        id: listFlick

        Column {
            id: content
            spacing: 6

            ListGroupHeader { width: listFlick.width; title: qsTr("Event log:") }
            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                CalaosItemBase {
                    text: qsTr("Show log")
                    width: 200 * calaosApp.density
                    height: parent.height
                    onButtonClicked: AppActions.openEventLog()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Connection:") }
            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                CalaosItemBase {
                    text: qsTr("Logout")
                    width: 200 * calaosApp.density
                    height: parent.height
                    onButtonClicked: calaosApp.logout()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                CalaosItemBase {
                    text: qsTr("Reset all data")
                    width: 200 * calaosApp.density
                    height: parent.height
                    onButtonClicked: calaosApp.resetAllData()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Favorites:") }

            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                CalaosItemBase {
                    text: qsTr("Add a favorite")
                    width: 200 * calaosApp.density
                    height: parent.height
                    onButtonClicked: favoriteAddClicked()
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            Item {
                width: listFlick.width;
                height: 40 * calaosApp.density
                CalaosItemBase {
                    text: qsTr("Edit favorites list")
                    width: 200 * calaosApp.density
                    height: parent.height
                    anchors.horizontalCenter: parent.horizontalCenter
                    onButtonClicked: favoriteEditClicked()
                }
            }

            ListGroupHeader { width: listFlick.width; title: qsTr("Language:") }

            QuickControls.ExclusiveGroup {
                id: optionGroup
            }

            Repeater {
                model: langModel

                CalaosRadio {
                    property variant langItemModel

                    Component.onCompleted: {
                        langItemModel = Qt.binding(function() { return langModel.getLangModel(model.index) })
                        checked = langItemModel.langActive
                    }

                    text: langItemModel.langName
                    exclusiveGroup: optionGroup
                    onCheckedChanged: {
                        langItemModel.langActive = checked
                        if (checked) {
                            calaosApp.setLanguage(langItemModel.langCode)
                        }
                    }
                }
            }

        }

        contentHeight: content.implicitHeight
        contentWidth: parent.width
        width: parent.width
        height: parent.height - header.height
        y: header.height
        flickableDirection: Flickable.VerticalFlick
    }

    ScrollBar { listObject: listFlick }

    ViewHeader {
        id: header
        headerLabel: qsTr("Settings")
        iconSource: calaosApp.getPictureSized("icon_config")
    }
}
