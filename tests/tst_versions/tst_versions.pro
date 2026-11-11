TEMPLATE = app
TARGET = tst_versions

include(../common.pri)

# The versions page: the reading of the daemon and relay answers and the copied
# text (VersionsReport), the page state (VersionsModel), and the kiosk route on
# the real CalaosOsAPI against the in-process fake daemon of tst_boxtime.

# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, without ScreenManager and the desktop config files.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets quick

INCLUDEPATH += $$PWD/../tst_boxtime

HEADERS += \
    $$PWD/../tst_boxtime/FakeTimeDaemon.h \
    $$SRC_DIR/VersionsReport.h \
    $$SRC_DIR/VersionsModel.h \
    $$SRC_DIR/CalaosOsAPI.h \
    $$SRC_DIR/CalaosOsProtocol.h \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/AsyncJobs.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h

SOURCES += \
    $$SRC_DIR/VersionsReport.cpp \
    $$SRC_DIR/VersionsModel.cpp \
    $$SRC_DIR/CalaosOsAPI.cpp \
    $$SRC_DIR/CalaosOsProtocol.cpp \
    $$SRC_DIR/NetworkRequest.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/AsyncJobs.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_versions.cpp
