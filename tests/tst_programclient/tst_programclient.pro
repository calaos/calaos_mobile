TEMPLATE = app
TARGET = tst_programclient

include(../common.pri)

# ProgramClient (time ranges and program states of the climate program
# editor) against a fake transport; CalaosConnection's tracked requests
# without a session.
DEFINES += CALAOS_MOBILE
QT += widgets

HEADERS += \
    $$SRC_DIR/ProgramTransport.h \
    $$SRC_DIR/ProgramClient.h \
    $$SRC_DIR/AdminTransport.h \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/ProgramClient.cpp \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_programclient.cpp
