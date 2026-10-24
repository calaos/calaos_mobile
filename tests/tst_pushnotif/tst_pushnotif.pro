TEMPLATE = app
TARGET = tst_pushnotif

include(../common.pri)

# Push notification plumbing shared by iOS and Android (FIX-3):
#  - PushRegistration (header-only, in CalaosConnection.h): when register_push
#    must be sent;
#  - HardwareUtils (base class): deferred notification tap, token changes;
#  - a real CalaosConnection against in-process fake servers (HTTP and
#    websocket): register_push at login, on token rotation, never twice.
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, exactly the code iOS and Android inherit.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

# Debug build on purpose: CalaosConnection::sendHttp() only dumps the JSON it
# sends under QT_DEBUG, and that dump is what must not leak the push token.
CONFIG -= release
CONFIG += debug

HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/HardwareUtils.h \
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
    tst_pushnotif.cpp
