TEMPLATE = app
TARGET = tst_notifquery

include(../common.pri)

# URLs of the iOS notification extension (ios/PushNotif/CalaosQuery.m): the
# credentials must reach api.php intact whatever they contain. The extension
# links no Qt; only the test does. macOS only (tests.pro).
LIBS += -framework Foundation
QMAKE_OBJECTIVE_CFLAGS += -fobjc-arc

INCLUDEPATH += $$PWD/../../ios/PushNotif

HEADERS += $$PWD/../../ios/PushNotif/CalaosQuery.h

OBJECTIVE_SOURCES += \
    $$PWD/../../ios/PushNotif/CalaosQuery.m \
    tst_notifquery.mm
