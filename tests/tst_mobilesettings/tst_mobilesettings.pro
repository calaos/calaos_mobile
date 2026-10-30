TEMPLATE = app
TARGET = tst_mobilesettings

include(../common.pri)

# Qt Quick Test of the mobile settings page (qml/mobile/SettingsView.qml)
# and of the box updates page (qml/mobile/UpdatesView.qml)
# with stand-ins for the context properties they read; the QuickFlux stub is
# the one of tst_climatewidget.
QT += quick qmltest
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"

# The real images (the clear icon of the text field).
RESOURCES += $$PWD/../../img_mobile.qrc

# The bottom sheets import the Calaos module (Common).
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_mobilesettings.cpp

OTHER_FILES += \
    tst_settingsdevicename.qml \
    tst_updatesview.qml
