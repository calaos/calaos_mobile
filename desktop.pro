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

QT += widgets

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
    src/ScreenManager.cpp
HEADERS += src/HardwareUtils_desktop.h \
    src/CalaosWidget.h \
    src/CalaosWidgetModel.h \
    src/WeatherInfo.h \
    src/XUtils.h \
    src/ScreenManager.h

RESOURCES += qml_desktop.qrc \
    img_desktop.qrc \
    fonts_desktop.qrc \
    qml_shared.qrc \
    widgets.qrc \
    lang.qrc \
    qml/desktop/keyboard_style/calaos_style.qrc
