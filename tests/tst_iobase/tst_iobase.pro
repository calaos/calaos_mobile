TEMPLATE = app
TARGET = tst_iobase

include(../common.pri)

# T31 : IOBase testée seule. La liste ci-dessous est le livrable du ticket —
# avant, aucun test ne pouvait instancier un IOBase sans lier en plus
# HardwareUtils (+ QT += widgets) ou quickflux, parce que askStateText()
# ouvrait le dialogue depuis le modèle.
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOConnection.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/RoomModel.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/RoomModel.cpp \
    tst_iobase.cpp
