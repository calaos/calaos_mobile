import QtQuick 2.2
import Units 1.0
import QtQuick.Layouts 1.1

Item {
    id: menu

    anchors.fill: parent

    Image {
        source: "qrc:/img/menu_header_shadow.png"
        opacity: 0.588

        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop

        anchors {
            top: headerBg.bottom
            left: parent.left
            right: parent.right
        }
    }

    Image {
        id: headerBg
        source: "qrc:/img/menu_header_background.png"

        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop

        anchors {
            top: parent.top;
            left: parent.left
            right: parent.right
            bottom: parent.top; bottomMargin: Units.dp(-22)
        }

        Image {
            source: "qrc:/img/menu_header_background_version.png"
            anchors { top: parent.top; right: parent.right }
        }

        Text {
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
            anchors.rightMargin: Units.dp(5)
            text: "v2.99+git"
            font.pixelSize: Units.dp(10)
            color: "#5f5f5f"
            font.family: calaosFont.fontFamilyCalaosTextBold
        }

        Image {
            source: "qrc:/img/menu_logo.png"
            anchors {
                left: parent.left; leftMargin: Units.dp(8)
                verticalCenter: parent.verticalCenter
            }
        }
    }

    Image {
        source: "qrc:/img/menu_footer_shadow.png"
        opacity: 0.588

        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop

        anchors {
            bottom: footerBg.top
            left: parent.left
            right: parent.right
        }
    }

    Image {
        id: footerBg
        source: "qrc:/img/menu_footer_background.png"

        fillMode: Image.Tile
        horizontalAlignment: Image.AlignLeft
        verticalAlignment: Image.AlignTop

        anchors {
            top: parent.bottom; topMargin: Units.dp(-98)
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Image {
            source: "qrc:/img/menu_footer_background_left.png"
            anchors {
                top: parent.top; bottom: parent.bottom
                left: parent.left
            }
        }
        Image {
            source: "qrc:/img/menu_footer_background_right.png"
            anchors {
                top: parent.top; bottom: parent.bottom
                right: parent.right
            }
        }
    }

    Image {
        source: "qrc:/img/menu_buttons_shadow_left.png"
        anchors {
            right: menuLayout.left
            top: footerBg.top; bottom: parent.bottom
        }
        opacity: 0.588
    }
    Image {
        source: "qrc:/img/menu_buttons_shadow_right.png"
        anchors {
            left: menuLayout.right
            top: footerBg.top; bottom: parent.bottom
        }
        opacity: 0.588
    }

    Image {
        id: glow
        source: "qrc:/img/menu_glowed_neon.png"
        opacity: currentButton == 0?0.8:0

        anchors {
            verticalCenter: footerBg.top; verticalCenterOffset: Units.dp(1)
            left: parent.left
            right: parent.right
        }

        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.OutQuad } }
    }

    property int currentButton: 0

    RowLayout {
        id: menuLayout

        spacing: 0
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
        }

        MainMenuButton {
            iconName: "home"
            buttonLabel: qsTr("My Home")

            selected: currentButton == 1
            onClicked: currentButton = 1
        }

        MainMenuButton {
            iconName: "media"
            buttonLabel: qsTr("Media")

            selected: currentButton == 2
            onClicked: currentButton = 2
        }

        MainMenuButton {
            iconName: "scenarios"
            buttonLabel: qsTr("Scenarios")

            selected: currentButton == 3
            onClicked: currentButton = 3
        }

        MainMenuButton {
            iconName: "configuration"
            buttonLabel: qsTr("Configuration")

            selected: currentButton == 4
            onClicked: currentButton = 4
        }
    }
}
