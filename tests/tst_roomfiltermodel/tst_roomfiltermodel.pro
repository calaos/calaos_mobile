TEMPLATE = app
TARGET = tst_roomfiltermodel

include(../common.pri)

# RoomFilterModel manipule RoomModel/IOBase par dynamic_cast, mais
# src/RoomModel.cpp tire HardwareUtils + quickflux + QtQuick. On lie donc le
# vrai src/RoomModel.h (déclarations + moc, donc les typeinfo attendus par
# dynamic_cast) avec la doublure de lien roommodel_stub.cpp.
# Ne PAS ajouter $$SRC_DIR/RoomModel.cpp ici.
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/RoomFilterModel.h \
    $$SRC_DIR/RoomModel.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/RoomFilterModel.cpp \
    roommodel_stub.cpp \
    tst_roomfiltermodel.cpp
