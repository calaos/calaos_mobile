TEMPLATE = app
TARGET = tst_httpreplies

include(../common.pri)

# Lifetime of the HTTP replies CalaosConnection keeps in reqReplies (FIX-3):
# a reply that fails must leave the list, otherwise the next resetTransport()
# (logout, reconnection) aborts a deleted object.
#
# Built with AddressSanitizer: a use-after-free is undefined behaviour and
# does not reliably crash, ASan turns it into a deterministic failure (the
# Qt libraries are not instrumented, CalaosConnection.cpp is, and that is
# where the freed reply is touched).
CONFIG += sanitizer sanitize_address

# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, as in tst_pushnotif.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_httpreplies.cpp
