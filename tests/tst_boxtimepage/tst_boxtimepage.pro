TEMPLATE = app
TARGET = tst_boxtimepage

include(../common.pri)

# Qt Quick Test of the box time page of the kiosk (qml/desktop/ConfigTimeView.qml)
# on the real BoxTimeManager and CalaosOsAPI, against the fake daemon of
# tst_boxtime; the QuickFlux stub is the one of tst_climatewidget.
QT += quick qmltest widgets
CONFIG += qmltestcase

# Same reason as tst_boxtime: HardwareUtils stays the plain base class.
DEFINES += CALAOS_MOBILE

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"
DEFINES += CALAOS_FONTS_DIR=\\\"$$clean_path($$PWD/../../fonts)\\\"

RESOURCES += $$PWD/../../img_desktop.qrc

INCLUDEPATH += $$PWD/../tst_boxtime

HEADERS += \
    $$PWD/../tst_boxtime/FakeTimeDaemon.h \
    $$SRC_DIR/BoxTime.h \
    $$SRC_DIR/BoxTimeManager.h \
    $$SRC_DIR/CalaosOsAPI.h \
    $$SRC_DIR/CalaosOsProtocol.h \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/AsyncJobs.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/BoxTime.cpp \
    $$SRC_DIR/BoxTimeManager.cpp \
    $$SRC_DIR/CalaosOsAPI.cpp \
    $$SRC_DIR/CalaosOsProtocol.cpp \
    $$SRC_DIR/NetworkRequest.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/AsyncJobs.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_boxtimepage.cpp

OTHER_FILES += \
    tst_configtimeview.qml
