#Calaos common project files

isEmpty(PREFIX) {
 PREFIX = /usr/local
}
TARGET.path = $$PREFIX/

QT += qml quick network gui websockets

CONFIG += c++11

include(3rd_party/quickflux-1.0.6/quickflux.pri)
include(3rd_party/qt-qml-models/QtQmlModels.pri)

SOURCES += src/main.cpp \
    src/HomeModel.cpp \
    src/Application.cpp \
    src/CalaosConnection.cpp \
    src/Common.cpp \
    src/RoomModel.cpp \
    src/AudioModel.cpp \
    src/FavoritesModel.cpp \
    src/HardwareUtils.cpp \
    src/CameraModel.cpp \
    $$PWD/src/RoomFilterModel.cpp \
    $$PWD/src/Machine.cpp \
    $$PWD/src/Lang.cpp \
    $$PWD/src/EventLogModel.cpp

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH = qml

HEADERS += \
    src/HomeModel.h \
    src/Application.h \
    src/CalaosConnection.h \
    src/Common.h \
    src/RoomModel.h \
    src/AudioModel.h \
    src/FavoritesModel.h \
    src/HardwareUtils.h \
    src/CameraModel.h \
    $$PWD/src/RoomFilterModel.h \
    $$PWD/src/Machine.h \
    $$PWD/src/qqmlhelpers.h \
    $$PWD/src/Lang.h \
    $$PWD/src/EventLogModel.h

win32 {
LIBS += -liphlpapi
}
