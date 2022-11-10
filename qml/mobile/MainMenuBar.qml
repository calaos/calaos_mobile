import QtQuick
import Calaos
import SharedComponents

Image {

    height: menuType === Common.MenuMain?
                98 / 2 * calaosApp.density:
                calaosApp.needBackButton?
                    48 * calaosApp.density:
                    0

    Behavior on height { NumberAnimation { easing.type: Easing.OutExpo; duration: 500 } }

    fillMode: Image.TileHorizontally
    verticalAlignment: Image.AlignLeft

    source: "qrc:/img/menu_footer_background.png"

    property int menuType: Common.MenuMain

    signal buttonHomeClicked()
    signal buttonMediaClicked()
    signal buttonScenariosClicked()
    signal buttonConfigClicked()
    signal buttonBackClicked()

    onMenuTypeChanged: if (menuType === Common.MenuMain) unselectAll()

    function unselectAll(bt) {
        if (bt !== btHome) btHome.selected = false
        if (bt !== btMedia) btMedia.selected = false
        if (bt !== btScenario) btScenario.selected = false
        if (bt !== btConfig) btConfig.selected = false
    }

    ButtonFooter {

        opacity: menuType === Common.MenuBack?1:0
        Behavior on opacity { NumberAnimation {} }
        visible: calaosApp.needBackButton?opacity > 0:false

        icon: calaosApp.getPictureSized("icon_exit")
        buttonLabel: qsTr("Back")
        onButtonClicked: {
            unselectAll()
            buttonBackClicked()
        }
        anchors {
            verticalCenter: parent.verticalCenter
            left: parent.left
            leftMargin: 10 * calaosApp.density
        }
    }

    Image {
        source: "qrc:/img/menu_footer_background_left.png"

        width: 2 * calaosApp.density
        height: parent.height

        anchors {
            right: row.left
            verticalCenterOffset: 1
        }
        opacity: row.opacity
    }

    Row {
        id: row

        opacity: menuType === Common.MenuMain?1:0
        Behavior on opacity { NumberAnimation {} }
        visible: opacity > 0

        height: btHome.height
        width: btHome.width * 4
        anchors {
            centerIn: parent
            verticalCenterOffset: !calaosApp.needBackButton && menuType === Common.MenuBack?height/2:1
        }
        Behavior on anchors.verticalCenterOffset { NumberAnimation { easing.type: Easing.OutExpo; duration: 500 } }

        MainMenuButton {
            id: btHome

            onButtonClicked: {
                unselectAll(btHome)
                buttonHomeClicked()
            }

            label: qsTr("Home")
            iconBase: "qrc:/img/button_home.png"
            iconGlow: "qrc:/img/button_home_glow.png"
            iconBloom: "qrc:/img/button_home_bloom.png"
        }

        MainMenuButton {
            id: btMedia

            onButtonClicked: {
                unselectAll(btMedia)
                buttonMediaClicked()
            }

            label: qsTr("Media")
            iconBase: "qrc:/img/button_media.png"
            iconGlow: "qrc:/img/button_media_glow.png"
            iconBloom: "qrc:/img/button_media_bloom.png"
        }

        MainMenuButton {
            id: btScenario

            onButtonClicked: {
                unselectAll(btScenario)
                buttonScenariosClicked()
            }

            label: qsTr("Scenarios")
            iconBase: "qrc:/img/button_scenarios.png"
            iconGlow: "qrc:/img/button_scenarios_glow.png"
            iconBloom: "qrc:/img/button_scenarios_bloom.png"
        }

        MainMenuButton {
            id: btConfig

            onButtonClicked: {
                unselectAll(btConfig)
                buttonConfigClicked()
            }

            label: qsTr("Config")
            iconBase: "qrc:/img/button_configuration.png"
            iconGlow: "qrc:/img/button_configuration_glow.png"
            iconBloom: "qrc:/img/button_configuration_bloom.png"
        }

    }

    Image {
        source: "qrc:/img/menu_footer_background_right.png"

        width: 2 * calaosApp.density
        height: parent.height

        anchors {
            left: row.right
            verticalCenterOffset: 1
        }
        opacity: row.opacity
    }
}
