TEMPLATE = app
TARGET = tst_homemodel

include(../common.pri)

# LightOnModel::addLight()/removeLight() and IOBase::getItemModel() are
# Qt slots / Q_INVOKABLE, so moc's static metacall table references them (and
# therefore IOBase's cloneIO()/typeinfo) unconditionally, however narrow the
# test - the linker needs the whole call graph, not just what the test calls.
# IOBase is defined in src/RoomModel.cpp, which in turn only compiles
# IOBase::askStateText() against HardwareUtils (CALAOS_MOBILE, chosen here)
# or quickflux (~20 more files, the other branch) - HardwareUtils is the
# smaller of the two.
#
# T18: src/CalaosConnection.cpp and src/CalaosEventDecoder.cpp are NOT linked
# any more. IOBase used to take a CalaosConnection *, so its constructor alone
# dragged the whole transport (QNetworkAccessManager, QWebSocket, the
# reconnection policy, the event decoder) into a test that never opens a
# socket. It now takes the src/IOConnection.h interface and the test provides
# its own FakeConnection. For the same reason src/CalaosConnection.h must NOT
# be listed in HEADERS: moc'ing it would pull CalaosConnection's metaobject,
# and with it CalaosConnection.cpp, straight back in.
DEFINES += CALAOS_MOBILE
QT += widgets

# RoomModel.cpp unconditionally #includes <qfappdispatcher.h> (only the usage
# at IOBase::askStateText() is guarded by CALAOS_MOBILE, not the include) -
# the header must be reachable to compile, even though CALAOS_MOBILE means
# none of quickflux's .cpp symbols are actually needed at link time.
INCLUDEPATH += $$PWD/../../3rd_party/quickflux/src

HEADERS += \
    $$SRC_DIR/HomeModel.h \
    $$SRC_DIR/RoomModel.h \
    $$SRC_DIR/IOConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/HomeModel.cpp \
    $$SRC_DIR/RoomModel.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_homemodel.cpp
