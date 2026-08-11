TEMPLATE = app
TARGET = tst_favoritesmodel

include(../common.pri)

# FavoritesModel.cpp rebuilds its rows from IOCache::Instance() and
# IOBase::cloneIO(), both defined in src/RoomModel.cpp, so a real IOBase is
# needed. IOBase's slots / Q_INVOKABLE are referenced unconditionally by moc's
# static metacall table, which drags in CalaosConnection.cpp (IOBase's
# constructor) and, through IOBase::askStateText(), either HardwareUtils
# (CALAOS_MOBILE, chosen here) or quickflux (~20 more files). HomeModel.cpp is
# needed for RoomItem, used by HomeFavModel::load() in the same translation
# unit. Same link perimeter as tst_homemodel, see its comments and
# tests/README.md.
DEFINES += CALAOS_MOBILE
QT += widgets

# RoomModel.cpp unconditionally #includes <qfappdispatcher.h> (only the usage
# at IOBase::askStateText() is guarded by CALAOS_MOBILE, not the include).
INCLUDEPATH += $$PWD/../../3rd_party/quickflux/src

HEADERS += \
    $$SRC_DIR/FavoritesModel.h \
    $$SRC_DIR/HomeModel.h \
    $$SRC_DIR/RoomModel.h \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/Common.h

SOURCES += \
    $$SRC_DIR/FavoritesModel.cpp \
    $$SRC_DIR/HomeModel.cpp \
    $$SRC_DIR/RoomModel.cpp \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/Common.cpp \
    tst_favoritesmodel.cpp
