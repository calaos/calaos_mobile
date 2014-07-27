import QtQuick 2.0

Image {

    height: 98 / 2 * calaosApp.density

    fillMode: Image.TileHorizontally
    verticalAlignment: Image.AlignLeft

    source: "qrc:/img/menu_footer_background.png"

    signal buttonHomeClicked()
    signal buttonMediaClicked()
    signal buttonScenariosClicked()
    signal buttonConfigClicked()
    signal buttonBackClicked()

    function unselectAll(bt) {
        if (bt !== btHome) btHome.selected = false
        if (bt !== btMedia) btMedia.selected = false
        if (bt !== btScenario) btScenario.selected = false
        if (bt !== btConfig) btConfig.selected = false
    }

    Row {

        height: btHome.height
        width: btHome.width * 4
        anchors {
            centerIn: parent
            verticalCenterOffset: 1
        }

        MainMenuButton {
            id: btHome

            onButtonClicked: {
                unselectAll(btHome)
                buttonHomeClicked()
            }

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

            iconBase: "qrc:/img/button_configuration.png"
            iconGlow: "qrc:/img/button_configuration_glow.png"
            iconBloom: "qrc:/img/button_configuration_bloom.png"
        }
    }

}
