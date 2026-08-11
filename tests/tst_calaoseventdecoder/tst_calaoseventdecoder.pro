TEMPLATE = app
TARGET = tst_calaoseventdecoder

include(../common.pri)

# CalaosEventDecoder is pure decoding: no QObject instance, no network, no
# HardwareUtils. The only other source it needs is Common.cpp, for the
# defensive number parsing of T11 (Common::toDoubleSafe), which in turn needs
# IOTypeRegistry.cpp.
#
# Do NOT add CalaosConnection.cpp here: the whole point of T17 is that the
# protocol can be tested without the transport, and linking it would drag in
# HardwareUtils, quickflux and QtWebSockets for nothing.
HEADERS += \
    $$SRC_DIR/CalaosEventDecoder.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_calaoseventdecoder.cpp
