TEMPLATE = app
TARGET = tst_tlspinning

include(../common.pri)

# TLS trust on first use:
#  - TlsPinning (pure functions: host key, SPKI extraction and hash, the
#    decision) and TlsPinStore (persistence), against PEM certificates
#    committed under tests/data (public certificates only, no key);
#  - a real CalaosConnection against in-process TLS servers on 127.0.0.1
#    (websocket and HTTP), with keys generated for the run by the openssl CLI.
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): a bare host becomes wss:// + https://,
# and HardwareUtils::Instance() is the plain base class.
DEFINES += CALAOS_MOBILE
DEFINES += TESTDATA_DIR=\\\"$$PWD/../data\\\"

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/TlsPinning.h

SOURCES += \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    tst_tlspinning.cpp
