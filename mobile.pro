TEMPLATE = app

QT += qml quick network

SOURCES += src/main.cpp

RESOURCES += qml.qrc \
    img.qrc

OTHER_FILES += \
    qml/main.qml \
    qml/LoginView.qml \
    qml/StyleButtonDefault.qml \
    qml/Loading.qml \
    qml/calaos.js \
    qml/RoomListView.qml \
    qml/ScrollBar.qml

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)
