TEMPLATE = app
TARGET = tst_climatewidget

include(../common.pri)

# Qt Quick Test: ClimateLogic.js and IOClimate against a real IOBase, so that
# a rename of an IOBase property breaks this test and not just the app.
QT += quick qmltest
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$PWD/stubs\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"

HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOConnection.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/RoomModel.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/RoomModel.cpp \
    tst_climatewidget.cpp

OTHER_FILES += \
    tst_climatelogic.qml \
    tst_ioclimate.qml \
    stubs/QuickFlux/qmldir
