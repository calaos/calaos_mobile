TEMPLATE = app
TARGET = tst_httpapiversion

include(../common.pri)

# CalaosConnection::detectHttpApiV2() is a static, header-only function inside
# src/CalaosConnection.h, exactly like ReconnectPolicy, precisely so that the
# rule it encodes can be tested without a server and without the network stack.
#
# Do NOT add $$SRC_DIR/CalaosConnection.cpp here and do NOT list
# CalaosConnection.h in HEADERS: moc'ing that header would pull
# CalaosConnection's metaobject into the link, and with it HardwareUtils,
# Common and the whole QtWebSockets/QtNetwork call graph, for no benefit.
# The test only includes the header, it never instantiates CalaosConnection.
SOURCES += \
    tst_httpapiversion.cpp
