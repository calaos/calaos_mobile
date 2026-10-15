TEMPLATE = app
TARGET = tst_common

include(../common.pri)

# Common.cpp ne dépend que de QtCore/QtQml, et de IOTypeRegistry.cpp auquel
# Common::IOTypeToString/FromString délèguent (T16) — même dépendance nulle.
# Ne pas ajouter d'autres .cpp ici « au cas où » : cela ferait entrer
# HardwareUtils, quickflux et QtQuick dans le lien du test.
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_common.cpp
