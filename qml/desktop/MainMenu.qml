import QtQuick 2.2
import SharedComponents 1.0
import QtQuick.Layouts 1.1
import QuickFlux 1.0
import "../quickflux"

Item {
    id: menu

    signal buttonHomeClicked()
    signal buttonMediaClicked()
    signal buttonScenariosClicked()
    signal buttonConfigClicked()

    property bool homeboardOpened: false

    anchors.fill: parent

    function unselectAll() {
        currentButton = 0
    }

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
        source: "qrc:/img/glowed_556_line_final.png"

        anchors {
            bottom: footerBg.top; bottomMargin: Units.dp(10)
            horizontalCenter: parent.horizontalCenter
        }

        opacity: homeboardLinkVisible && !homeboardOpened?1:0

        Behavior on opacity { NumberAnimation {} }
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

    Rectangle {
        color: "#000000"
        anchors {
            top: headerBg.bottom
            bottom: footerBg.top
            left: parent.left; right: parent.right
        }

        opacity: homeboardOpened?0.5:0
        visible: opacity > 0
        Behavior on opacity { PropertyAnimation {} }

        MouseArea {
            anchors.fill: parent
            onClicked: homeboardOpened = false
            enabled: homeboardOpened
            visible: enabled
        }
    }

    Image {
        id: homeboardBack
        source: "qrc:/img/menu_back.png"

        anchors {
            horizontalCenter: parent.horizontalCenter
            top: homeboardArrow.bottom
        }

        visible: opacity > 0
        opacity: homeboardOpened?1:0
        Behavior on opacity { PropertyAnimation {} }

        HomeboardListView {
            anchors.fill: parent

            model: ListModel {
                ListElement {
                    iconSource: "IconWidget.qml"
                    title: qsTr("Add new Widgets")
                    description: qsTr("Add new widgets on the desktop")
                    moreText: ""
                    action: "widgets_add"
                }
                ListElement {
                    iconSource: "IconWidget.qml"
                    title: qsTr("Widgets configuration")
                    description: qsTr("Move and setup your widgets on the desktop")
                    moreText: ""
                    action: "widgets_edit"
                }
                ListElement {
                    iconSource: "IconScreensaver.qml"
                    title: qsTr("Shut the screen off")
                    description: qsTr("Touch the screen once to wake up the machine")
                    moreText: qsTr("Auto: off")
                    action: "screensaver"
                }
                ListElement {
                    iconSource: "IconReboot.qml"
                    title: qsTr("Reboot the machine")
                    description: qsTr("Please wait during the reboot")
                    moreText: ""
                    action: "reboot"
                }
            }
        }
    }

    AppListener {
        id: actHomeboard
        property bool forceHide: false
        Filter {
            type: ActionTypes.hideHomeboardMenu
            onDispatched: actHomeboard.forceHide = true
        }
        Filter {
            type: ActionTypes.showHomeboardMenu
            onDispatched: actHomeboard.forceHide = false
        }
    }

    property bool homeboardLinkVisible: currentButton == 0 && !actHomeboard.forceHide
    Item {
        id: homeboardArrow
        width: upArrow.width
        height: upArrow.height

        visible: opacity > 0
        opacity: homeboardLinkVisible?1:0
        Behavior on opacity { PropertyAnimation {} }

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: footerBg.top;
            bottomMargin: homeboardOpened?Units.dp(10) + homeboardBack.height:
                                           Units.dp(10)
        }
        Behavior on anchors.bottomMargin { PropertyAnimation { easing.type: Easing.OutCubic} }

        Image {
            id: upArrow
            source: "qrc:/img/button_glow_up.png"

            visible: opacity > 0
            opacity: homeboardOpened?0:1
            Behavior on opacity { PropertyAnimation {} }
        }
        Image {
            id: dnArrow
            source: "qrc:/img/button_glow_down.png"

            visible: opacity > 0
            opacity: upArrow.opacity > 0?0:1
            Behavior on opacity { PropertyAnimation {} }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: homeboardOpened = !homeboardOpened
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
            disabled: homeboardOpened
            onClicked: {
                buttonHomeClicked()
                currentButton = 1
            }
        }

        MainMenuButton {
            iconName: "media"
            buttonLabel: qsTr("Media")

            selected: currentButton == 2
            disabled: homeboardOpened
            onClicked: {
                buttonMediaClicked()
                currentButton = 2
            }
        }

        MainMenuButton {
            iconName: "scenarios"
            buttonLabel: qsTr("Scenarios")

            selected: currentButton == 3
            disabled: homeboardOpened
            onClicked: {
                buttonScenariosClicked()
                currentButton = 3
            }
        }

        MainMenuButton {
            iconName: "configuration"
            buttonLabel: qsTr("Configuration")

            selected: currentButton == 4
            disabled: homeboardOpened
            onClicked: {
                buttonConfigClicked()
                currentButton = 4
            }
        }
    }

    //Dispatch actions
    AppListener {
        Filter {
            type: ActionTypes.clickHomeboardItem
            onDispatched: homeboardOpened = false
        }
    }
}
