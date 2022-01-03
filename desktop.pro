#Calaos Home for desktop (calaos-os)

TEMPLATE = app

isEmpty(PREFIX) {
    PREFIX = /usr/local
}

DEFINES += CALAOS_DESKTOP
DEFINES += ETC_DIR=\\\"$$PREFIX/etc\\\"

include(calaos.pri)
include(lang/lang.pri)

TARGET=calaos_home

QT += core widgets network 

qtHaveModule(webengine) {
QT += webengine
DEFINES += HAVE_WEBENGINE
}

linux {
QT_CONFIG -= no-pkg-config
CONFIG += link_pkgconfig
PKGCONFIG += x11 xext
}

QML_IMPORT_PATH = $$PWD/qml

SOURCES += src/HardwareUtils_desktop.cpp \
    src/CalaosWidget.cpp \
    src/CalaosWidgetModel.cpp \
    src/WeatherInfo.cpp \
    src/XUtils.cpp \
    src/ScreenManager.cpp \
    src/UserInfoModel.cpp
HEADERS += src/HardwareUtils_desktop.h \
    src/CalaosWidget.h \
    src/CalaosWidgetModel.h \
    src/WeatherInfo.h \
    src/XUtils.h \
    src/ScreenManager.h \
    src/UserInfoModel.h

RESOURCES += qml_desktop.qrc \
    img_desktop.qrc \
    fonts_desktop.qrc \
    qml_shared.qrc \
    widgets.qrc \
    lang.qrc \
    qml/desktop/keyboard_style/calaos_style.qrc

lupdate_only {
SOURCES += \
    src/*.cpp \
    src/android/*.cpp \
    src/ios/*.cpp \
    src/ios/*.m \
    src/ios/*.mm

HEADERS += \
    src/*.h \
    src/android/*.h \
    src/ios/*.h

SOURCES += \
    qml/desktop/*.qml \
    qml/desktop/*.js \
    qml/mobile/*.qml \
    qml/mobile/*.js \
    qml/quickflux/*.qml \
    qml/quickflux/*.js \
    qml/SharedComponents/*.qml \
    qml/SharedComponents/*.js \
    widgets/*/*.qml \
    widgets/*/*.js
}
