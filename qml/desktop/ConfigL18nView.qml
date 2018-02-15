import QtQuick 2.5
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import QtQuick.Controls 1.3 as QuickControls

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

    BorderImage {

        source: "qrc:/img/standard_list_decoration.png"

        border {
            left: Units.dp(27); right: Units.dp(27)
            top: Units.dp(50); bottom: Units.dp(50)
        }

        anchors {
            left: parent.left; leftMargin: Units.dp(20)
            right: tabs.left; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
        }

        ColumnLayout {

            anchors {
                left: parent.left; leftMargin: Units.dp(60)
                right: parent.right; rightMargin: Units.dp(60)
                verticalCenter: parent.verticalCenter
            }

            spacing: Units.dp(10)

            Text {
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(15) }
                color: Theme.colorAlpha(Theme.whiteColor, 0.7)
                text: qsTr("Choose the desired language for the user interface from the list below:")
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }

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

            Text {
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(14) }
                color: Theme.blueColor
                text: qsTr("* Restart required to apply changes")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true

                //Message is visible only if dynamic qml strings is available (>= qt5.10)
                visible: !langModel.hasDynamicLangChange
            }

            Text {
                font { family: calaosFont.fontFamily; weight: Font.ExtraLight; pixelSize: Units.dp(12) }
                color: Theme.blueColor
                text: qsTr("If your language is not listed here or if you found some mistake in the translation, please contact the Calaos development team to help us improve the software.")
                wrapMode: Text.WordWrap

                Layout.fillWidth: true
            }
        }

    }

    ConfigTabs {
        id: tabs
        width: Units.dp(300)

        anchors {
            right: parent.right; rightMargin: Units.dp(20)
            top: header.bottom; topMargin: Units.dp(20)
            bottom: footer.top; bottomMargin: Units.dp(20)
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
            text: qsTr("Localization configuration")
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
                label: qsTr("Back to config")
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
