TEMPLATE = app

TARGET=CalaosHome

QT += qml quick network

android {
    QT += androidextras
}

ios {
    QMAKE_INFO_PLIST = ios/AppInfo.plist
    ICON.files = ios/Default.png \
    ios/Default@2x.png \
    ios/Icon-72.png \
    ios/Icon-Small-50.png \
    ios/Icon-Small.png \
    ios/Icon-Small@2x.png \
    ios/Icon.png \
    ios/Icon@2x.png
    QMAKE_BUNDLE_DATA += ICON
}

CONFIG += c++11

SOURCES += src/main.cpp \
    src/HomeModel.cpp \
    src/Application.cpp \
    src/CalaosConnection.cpp \
    src/Common.cpp \
    src/RoomModel.cpp \
    src/AudioModel.cpp

RESOURCES += qml.qrc \
    img.qrc

OTHER_FILES += \
    qml/main.qml \
    qml/LoginView.qml \
    qml/StyleButtonDefault.qml \
    qml/Loading.qml \
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
    ios/Default.png \
    ios/Default@2x.png \
    ios/Icon-72.png \
    ios/Icon-Small-50.png \
    ios/Icon-Small.png \
    ios/Icon-Small@2x.png \
    ios/Icon.png \
    ios/Icon@2x.png \
    ios/AppInfo.plist \
    qml/ScenarioView.qml \
    qml/RoomDetailView.qml \
    qml/MediaMenuView.qml \
    qml/IOLightDimmer.qml \
    qml/IOLightRGB.qml \
    qml/IOShutterSmart.qml

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

HEADERS += \
    src/HomeModel.h \
    src/qqmlhelpers.h \
    src/Application.h \
    src/CalaosConnection.h \
    src/Common.h \
    src/RoomModel.h \
    src/AudioModel.h
