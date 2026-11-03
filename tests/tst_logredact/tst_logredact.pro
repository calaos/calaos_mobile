TEMPLATE = app
TARGET = tst_logredact

include(../common.pri)

# What NetworkRequest and WeatherModel write to the log: no key, password or
# token, on the request line, the error text, the error body and the success dump.
DEFINES += CALAOS_MOBILE
QT += widgets

HEADERS += \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/LogRedact.h \
    $$SRC_DIR/WeatherInfo.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/TlsPinning.h

SOURCES += \
    $$SRC_DIR/NetworkRequest.cpp \
    $$SRC_DIR/WeatherInfo.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    tst_logredact.cpp
