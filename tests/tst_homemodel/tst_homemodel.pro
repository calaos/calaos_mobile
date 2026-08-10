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
# smaller of the two. CalaosConnection.cpp is needed for IOBase's constructor
# and its other Q_INVOKABLE slots. This is far more than "lier le minimum de
# sources" (tests/README.md); see tst_homemodel.cpp's header comment and the
# T06 report for why it could not be reduced further.
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
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h

SOURCES += \
    $$SRC_DIR/HomeModel.cpp \
    $$SRC_DIR/RoomModel.cpp \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    tst_homemodel.cpp
