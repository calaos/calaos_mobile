TEMPLATE = app
TARGET = tst_mobilesettings

include(../common.pri)

# Qt Quick Test of the mobile settings pages (qml/mobile/Settings*.qml),
# of the box updates page (qml/mobile/UpdatesView.qml) and of the admin code
# sheet (qml/mobile/AdminCodeSheet.qml) with stand-ins for the context
# properties they read; the QuickFlux stub is the one of tst_climatewidget.
QT += quick qmltest
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"
DEFINES += CALAOS_FONTS_DIR=\\\"$$clean_path($$PWD/../../fonts)\\\"
DEFINES += CALAOS_LANG_DIR=\\\"$$clean_path($$PWD/../../lang)\\\"

# The real images (the clear icon of the text field, the captures); the QML
# resources tell whether the pages reach the app.
RESOURCES += \
    $$PWD/../../img_mobile.qrc \
    $$PWD/../../qml_mobile.qrc

# The bottom sheets import the Calaos module (Common).
HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_mobilesettings.cpp

OTHER_FILES += \
    tst_keyboardscroller.qml \
    tst_settingsdevicename.qml \
    tst_settingsnavigation.qml \
    tst_settingseventlog.qml \
    tst_admincodesheet.qml \
    tst_updatesview.qml
