TEMPLATE = app
TARGET = tst_common

include(../common.pri)

# Common.cpp ne dépend que de QtCore/QtQml : aucune autre source de src/ n'est
# nécessaire. Ne pas ajouter d'autres .cpp ici « au cas où » : cela ferait
# entrer HardwareUtils, quickflux et QtQuick dans le lien du test.
HEADERS += \
    $$SRC_DIR/Common.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    tst_common.cpp
