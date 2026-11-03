TEMPLATE = app
TARGET = tst_netquiet

include(../common.pri)

# Separate process: the logging rules must be the defaults, which the other
# suites override with "*.debug=true".
DEFINES += CALAOS_MOBILE
QT += widgets

HEADERS += \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/LogRedact.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/qqmlhelpers.h

SOURCES += \
    $$SRC_DIR/NetworkRequest.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_netquiet.cpp
