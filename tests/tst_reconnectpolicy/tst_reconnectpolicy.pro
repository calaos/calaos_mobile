TEMPLATE = app
TARGET = tst_reconnectpolicy

include(../common.pri)

# ReconnectPolicy is header-only, inside src/CalaosConnection.h, precisely so
# that this test can drive the reconnection state machine and the backoff
# without a server and without the network stack.
#
# Do NOT add $$SRC_DIR/CalaosConnection.cpp here and do NOT list
# CalaosConnection.h in HEADERS: moc'ing that header would pull
# CalaosConnection's metaobject into the link, and with it HardwareUtils,
# Common and the whole QtWebSockets/QtNetwork call graph, for no benefit.
# The test only includes the header, it never instantiates CalaosConnection.
SOURCES += \
    tst_reconnectpolicy.cpp
