TEMPLATE = app
TARGET = tst_roomfiltermodel

include(../common.pri)

# T31: le vrai src/RoomModel.cpp est lié ici. Il n'y a plus de doublure.
# IOBase n'ouvre plus elle-même la boîte de saisie (askStateText tirait
# HardwareUtils sur mobile et quickflux sur desktop, sans garde à l'include) :
# c'est désormais le QML de chaque variante qui décide, via l'action
# ActionTypes.openAskTextForIo. RoomModel.cpp ne dépend donc plus que de
# Common, IOTypeRegistry, JsonKeys et de l'interface IOConnection.
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOConnection.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/JsonKeys.h \
    $$SRC_DIR/RoomFilterModel.h \
    $$SRC_DIR/RoomModel.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/RoomFilterModel.cpp \
    $$SRC_DIR/RoomModel.cpp \
    tst_roomfiltermodel.cpp
