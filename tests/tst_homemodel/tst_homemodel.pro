TEMPLATE = app
TARGET = tst_homemodel

include(../common.pri)

# LightOnModel::addLight()/removeLight() and IOBase::getItemModel() are
# Qt slots / Q_INVOKABLE, so moc's static metacall table references them (and
# therefore IOBase's cloneIO()/typeinfo) unconditionally, however narrow the
# test - the linker needs the whole call graph, not just what the test calls.
# IOBase is defined in src/RoomModel.cpp.
#
# T18: src/CalaosConnection.cpp and src/CalaosEventDecoder.cpp are NOT linked
# any more. IOBase used to take a CalaosConnection *, so its constructor alone
# dragged the whole transport (QNetworkAccessManager, QWebSocket, the
# reconnection policy, the event decoder) into a test that never opens a
# socket. It now takes the src/IOConnection.h interface and the test provides
# its own FakeConnection. For the same reason src/CalaosConnection.h must NOT
# be listed in HEADERS: moc'ing it would pull CalaosConnection's metaobject,
# and with it CalaosConnection.cpp, straight back in.
#
# T31: src/HardwareUtils.cpp, QT += widgets, DEFINES += CALAOS_MOBILE and the
# INCLUDEPATH towards 3rd_party/quickflux are gone. IOBase::askStateText()
# opened the input dialog itself and its metacall entry dragged in either
# HardwareUtils (+QtWidgets for QInputDialog) or quickflux, whichever branch
# was selected. The QML of each variant now decides, through the
# ActionTypes.openAskTextForIo action, so this test links models only.

HEADERS += \
    $$SRC_DIR/HomeModel.h \
    $$SRC_DIR/RoomModel.h \
    $$SRC_DIR/IOConnection.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h

SOURCES += \
    $$SRC_DIR/HomeModel.cpp \
    $$SRC_DIR/RoomModel.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_homemodel.cpp
