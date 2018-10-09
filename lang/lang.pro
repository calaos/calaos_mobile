#This is not a real project, but a dummy project for translation tools
TEMPLATE = app
QT += qml quick network gui websockets svg
TARGET=dummy

DEFINES += CALAOS_DESKTOP

SOURCES += \
    ../src/*.cpp \
    ../src/android/*.cpp \
    ../src/ios/*.cpp \
    ../src/ios/*.m \
    ../src/ios/*.mm

HEADERS += \
    ../src/*.h \
    ../src/android/*.h \
    ../src/ios/*.h

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
