TEMPLATE = app
TARGET = tst_qqmlhelpers

include(../common.pri)

# qqmlhelpers.h est purement macro : le test instancie ses propres classes
# sonde plutôt que d'entraîner un modèle applicatif dans le lien.
# Common.h/.cpp n'entrent ici que pour Common::AudioStatusType, qui sert à
# couvrir le cas d'une propriété modèle typée enum (comme AudioPlayer::status).
# Common.cpp délègue à IOTypeRegistry.cpp, d'où sa présence — même dépendance
# nulle que tst_common, aucun QtQuick ni HardwareUtils.
HEADERS += \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_qqmlhelpers.cpp
