#This is not a real project, but a dummy project for translation tools
TEMPLATE = app
QT += qml quick network gui websockets svg
TARGET=dummy

DEFINES += CALAOS_DESKTOP

SOURCES += \
    ../src/*.cpp \
    ../android/*.cpp \
    ../ios/*.m \
    ../ios/*.mm

HEADERS += \
    ../src/*.h \
    ../android/*.h \
    ../ios/*.h

SOURCES += \
    ../qml/desktop/*.qml \
    ../qml/desktop/*.js \
    ../qml/mobile/*.qml \
    ../qml/mobile/*.js \
    ../qml/quickflux/*.qml \
    ../qml/quickflux/*.js \
    ../qml/SharedComponents/*.qml \
    ../qml/SharedComponents/*.js \
    ../widgets/*/*.qml \
    ../widgets/*/*.js

include(lang.pri)
