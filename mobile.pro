TEMPLATE = app

QT += qml quick network

android {
    QT += androidextras
}

CONFIG += c++11

SOURCES += src/main.cpp \
    src/HomeModel.cpp \
    src/Application.cpp \
    src/CalaosConnection.cpp \
    src/Common.cpp \
    src/RoomModel.cpp

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
    qml/ItemListView.qml

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
    src/RoomModel.h
