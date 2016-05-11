#Calaos Home for desktop (calaos-os)

TEMPLATE = app

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

DEFINES += CALAOS_DESKTOP
DEFINES += ETC_DIR=\\\"$$PREFIX/etc\\\"

include(calaos.pri)

TARGET=calaos_home

QT += widgets

SOURCES += src/HardwareUtils_desktop.cpp
HEADERS += src/HardwareUtils_desktop.h

RESOURCES += qml_desktop.qrc \
    img_desktop.qrc \
    fonts_desktop.qrc

OTHER_FILES += \
    qml/main_desktop.qml \
    qml/LoginView.qml \
    qml/StyleButtonDefault.qml \
    qml/calaos.js \
    qml/RoomListView.qml \
    qml/ScrollBar.qml \
    qml/ItemListView.qml \
    qml/IOLight.qml \
    qml/AnimatedIcon.qml \
    qml/ItemButtonAction.qml \
    qml/IOTemp.qml \
    qml/IOVarInt.qml \
    qml/IOVarBool.qml \
    qml/IOVarString.qml \
    qml/IOScenario.qml \
    qml/IOShutter.qml \
    qml/IconItem.qml \
    qml/MainMenuBar.qml \
    qml/MainMenuButton.qml \
    qml/FavoritesListView.qml \
    qml/ButtonFooter.qml \
    qml/MediaMenuItem.qml \
    qml/IconMusic.qml \
    qml/IconCamera.qml \
    qml/ViewHeader.qml \
    qml/MusicListView.qml \
    qml/IconMusicPlayer.qml \
    qml/SettingsView.qml \
    qml/ListGroupHeader.qml \
    qml/ScenarioView.qml \
    qml/RoomDetailView.qml \
    qml/MediaMenuView.qml \
    qml/IOLightDimmer.qml \
    qml/IOLightRGB.qml \
    qml/IOShutterSmart.qml \
    qml/FavoritesAddView.qml \
    qml/FavoritesEditView.qml \
    qml/CameraListView.qml \
    qml/CameraSingleView.qml \
    qml/Fonts.qml \
    qml/ItemBase.qml \
    qml/ButtonLogin.qml \
    qml/TextFieldStyle.qml \
    qml/CalaosItemBase.qml \
    qml/CalaosTextField.qml \
    qml/CalaosSlider.qml \
    qml/CalaosSliderStyle.qml \
    qml/ColorPickerView.qml \
    qml/ColorPickerRGBView.qml \
    qml/IOFavAllLights.qml \
    qml/BackgroundDesktop.qml \
    qml/MainMenu.qml \
    qml/MainMenuButton.qml
