TEMPLATE = app
TARGET = tst_mobilesettings

include(../common.pri)

# Qt Quick Test of the mobile settings page (qml/mobile/SettingsView.qml)
# with stand-ins for the context properties it reads; the QuickFlux stub is
# the one of tst_climatewidget.
QT += quick qmltest
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"

# The real images (the clear icon of the text field).
RESOURCES += $$PWD/../../img_mobile.qrc

SOURCES += \
    tst_mobilesettings.cpp

OTHER_FILES += \
    tst_settingsdevicename.qml
