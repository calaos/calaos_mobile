TEMPLATE = app
TARGET = tst_cameratoken

include(../common.pri)

# Camera pictures and server capabilities of a real CalaosConnection against
# an in-process calaos_server (WebSocket and api.php on one port): the token
# replaces cn_pass when the server announces camera_token, silence means an
# old server, and the v1 picture url is assembled without QString::arg().
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, as in tst_httpreplies.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/CameraToken.h \
    $$SRC_DIR/ServerCapabilities.h \
    $$SRC_DIR/AdminTransport.h \
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
    tst_cameratoken.cpp
