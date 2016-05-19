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

SOURCES += src/HardwareUtils_desktop.cpp \
    src/CalaosWidget.cpp \
    src/CalaosWidgetModel.cpp
HEADERS += src/HardwareUtils_desktop.h \
    src/CalaosWidget.h \
    src/CalaosWidgetModel.h

RESOURCES += qml_desktop.qrc \
    img_desktop.qrc \
    fonts_desktop.qrc \
    qml_shared.qrc \
    widgets.qrc
