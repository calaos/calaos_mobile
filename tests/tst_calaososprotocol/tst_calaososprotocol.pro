TEMPLATE = app
TARGET = tst_calaososprotocol

include(../common.pri)

# Pure wire rules of the calaos-container daemon API (envelope, legacy
# fallback, upgrade jobs): no network, no QObject.
HEADERS += $$SRC_DIR/CalaosOsProtocol.h
SOURCES += $$SRC_DIR/CalaosOsProtocol.cpp \
           tst_calaososprotocol.cpp
