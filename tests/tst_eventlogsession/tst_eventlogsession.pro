TEMPLATE = app
TARGET = tst_eventlogsession

include(../common.pri)

# The vptr check wants the typeinfo of the class behind a pointer, which this
# test deliberately does not link.
# qmake appends the sanitizer flags after the project file: repeat it there.
QMAKE_SANITIZE_UNDEFINED_CXXFLAGS += -fno-sanitize=vptr

# Event log over a real session (EVLOG-1): a real CalaosConnection and a real
# EventLogModel, wired like Application (homeLoaded -> refresh()), against a
# fake calaos_server websocket and a fake api.php in process. Every way a
# page request used to be lost for good (server restart, iOS background,
# server error, no session, silent server, HTTP transport) must end with the
# history loading again, without duplicates, and an answer must only ever
# finish the request it belongs to.
#
# CALAOS_MOBILE (no CALAOS_DESKTOP): HardwareUtils::Instance() returns the
# plain base class, as in tst_pushnotif.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android.
QT += widgets

# EventLogModel.cpp resolves the io of an event through IOCache (RoomModel.cpp,
# which drags quickflux in): the test defines the two lookups itself and
# returns no io, events then show as push notifications or unknown events.
HEADERS += \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/TlsPinning.h \
    $$SRC_DIR/EventLogModel.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/TlsPinning.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/EventLogModel.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_eventlogsession.cpp
