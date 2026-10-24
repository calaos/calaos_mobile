TEMPLATE = app
TARGET = tst_sessionlifecycle

include(../common.pri)

# Session lifecycle of a real CalaosConnection against an in-process fake
# api.php (FIX-7): a session closed on purpose (logout) is not reopened by the
# network coming back, by discovery or by the app returning to the
# foreground; an active session still reconnects. The decisions themselves
# are covered in tst_reconnectpolicy, this checks the wiring: real logins
# counted on the server side.
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, as in tst_pushnotif.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/LaunchLogin.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_sessionlifecycle.cpp
