TEMPLATE = app
TARGET = tst_updateclient

include(../common.pri)

# The kiosk update client (CalaosOsAPI + UpdateManager) against an in-process
# fake calaos-container daemon (HTTP and events WebSocket on one 127.0.0.1
# port): /api/v2 jobs, legacy fallback, bearer header, 401 token re-read, 409.
#
# UpdateManager dispatches its dialogs through QFAppDispatcher: quickflux is
# linked and the test reads the dispatched actions.
include(../../3rd_party/quickflux/quickflux.pri)

# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, without ScreenManager and the desktop config files.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android, and
# QQmlApplicationEngine needs a QGuiApplication; QApplication covers both.
QT += widgets quick

HEADERS += \
    $$SRC_DIR/CalaosOsAPI.h \
    $$SRC_DIR/CalaosOsProtocol.h \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/AsyncJobs.h \
    $$SRC_DIR/UpdateManager.h \
    $$SRC_DIR/UpdateModel.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h

SOURCES += \
    $$SRC_DIR/CalaosOsAPI.cpp \
    $$SRC_DIR/CalaosOsProtocol.cpp \
    $$SRC_DIR/NetworkRequest.cpp \
    $$SRC_DIR/AsyncJobs.cpp \
    $$SRC_DIR/UpdateManager.cpp \
    $$SRC_DIR/UpdateModel.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_updateclient.cpp
