TEMPLATE = app
TARGET = tst_admincode

include(../common.pri)

# Encrypted admin code push (calaos_base docs/08_http_api.md, "Code push
# chiffré"): the construction against the three golden vectors, the
# rejections, the expiry counted from the reception, and AdminCodeModel over
# the in-memory stash of the base HardwareUtils.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

HEADERS += \
    $$SRC_DIR/AdminCode.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/AdminCode.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_admincode.cpp
