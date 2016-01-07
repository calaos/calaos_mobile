#Calaos Home for mobile

TEMPLATE = app

DEFINES += CALAOS_MOBILE

TARGET=CalaosHome

QT += qml quick network gui websockets

include(calaos.pri)

android {
    QT += androidextras
    OTHER_FILES += android/src/fr/calaos/calaoshome/HardwareUtils.java
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    SOURCES += android/HardwareUtils_Android.cpp
    HEADERS += android/HardwareUtils_Android.h
    OTHER_FILES += \
        android/AndroidManifest.xml \
        android/res/drawable-hdpi/icon.png \
        android/res/drawable-ldpi/icon.png \
        android/res/drawable-mdpi/icon.png
} else:ios {
    QMAKE_INFO_PLIST = ios/AppInfo.plist
    ICON.files = $$PWD/ios/Default.png \
    $$PWD/ios/Default@2x.png \
    $$PWD/ios/Default-568h@2x.png \
    $$PWD/ios/Icon-72.png \
    $$PWD/ios/Icon-Small-50.png \
    $$PWD/ios/Icon-Small.png \
    $$PWD/ios/Icon-Small@2x.png \
    $$PWD/ios/Icon.png \
    $$PWD/ios/Icon@2x.png \
    $$PWD/ios/Icon-76.png \
    $$PWD/ios/Icon-120.png \
    $$PWD/ios/Icon-152.png
    QMAKE_BUNDLE_DATA += ICON

    HEADERS += ios/HardwareUtils_iOS.h
    OBJECTIVE_SOURCES += ios/HardwareUtils.mm \
        ios/Reachability.h \
        ios/Reachability.m \
        ios/KeychainItemWrapper.h \
        ios/KeychainItemWrapper.m \
        ios/AlertPrompt.h \
        ios/AlertPrompt.m

    OTHER_FILES += \
        ios/Default-568h@2x.png \
        ios/Icon-76.png \
        ios/Icon-120.png \
        ios/Icon-152.png \
        ios/Default.png \
        ios/Default@2x.png \
        ios/Icon-72.png \
        ios/Icon-Small-50.png \
        ios/Icon-Small.png \
        ios/Icon-Small@2x.png \
        ios/Icon.png \
        ios/Icon@2x.png \
        ios/AppInfo.plist
} else {
    QT += widgets
}

RESOURCES += qml_mobile.qrc \
    img_mobile.qrc \
    fonts_mobile.qrc

OTHER_FILES += \
    qml/main_mobile.qml \
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
    qml/IOFavAllLights.qml \
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
    qml/ColorPickerRGBView.qml

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)
