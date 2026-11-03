TEMPLATE = app
TARGET = tst_networkbounds

include(../common.pri)

# A body larger than the bound, announced or not, must end the request with
# an error instead of being buffered: NetworkRequest (JSON and raw) and WeatherModel.
DEFINES += CALAOS_MOBILE
QT += widgets

HEADERS += \
    $$SRC_DIR/NetworkRequest.h \
    $$SRC_DIR/NetworkLimits.h \
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
    tst_networkbounds.cpp
