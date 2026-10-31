TEMPLATE = app
TARGET = tst_boxtime

include(../common.pri)

# The wall-clock rules of the box time page (BoxTime) and the page state
# (BoxTimeManager) on the real CalaosOsAPI, against an in-process fake
# calaos-container daemon.

# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, without ScreenManager and the desktop config files.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets quick

HEADERS += \
    FakeTimeDaemon.h \
    $$SRC_DIR/BoxTime.h \
    $$SRC_DIR/BoxTimeManager.h \
    $$SRC_DIR/CalaosOsAPI.h \
    $$SRC_DIR/CalaosOsProtocol.h \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/AsyncJobs.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h

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
    tst_boxtime.cpp
