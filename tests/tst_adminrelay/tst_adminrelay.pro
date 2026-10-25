TEMPLATE = app
TARGET = tst_adminrelay

include(../common.pri)

# Box updates from the phone (CM1): AdminRelay (admin_unlock / admin
# correlation, timeouts, gates, session lifecycle) against a fake transport
# and against a real CalaosConnection talking to a fake calaos_server
# websocket on 127.0.0.1; AdminRelayBackend + AdminUpdates mapping the
# relayed <Status>/<Job> onto the shared update models.
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, as in tst_pushnotif.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/AdminRelay.h \
    $$SRC_DIR/AdminRelayBackend.h \
    $$SRC_DIR/AdminUpdates.h \
    $$SRC_DIR/UpdateBackend.h \
    $$SRC_DIR/UpdateModel.h \
    $$SRC_DIR/UpdateGrouping.h \
    $$SRC_DIR/CalaosOsProtocol.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/AdminRelay.cpp \
    $$SRC_DIR/AdminRelayBackend.cpp \
    $$SRC_DIR/AdminUpdates.cpp \
    $$SRC_DIR/UpdateModel.cpp \
    $$SRC_DIR/CalaosOsProtocol.cpp \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_adminrelay.cpp
