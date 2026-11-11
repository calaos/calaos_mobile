TEMPLATE = app
TARGET = tst_versionspage

include(../common.pri)

# Qt Quick Test of the versions page of the kiosk (qml/desktop/ConfigVersionsView.qml)
# and of the "All versions" button of ConfigTabs, on the real VersionsModel
# driven by a fetcher the test answers by hand; the QuickFlux stub is the one of
# tst_climatewidget.
QT += quick qmltest widgets
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"
DEFINES += CALAOS_FONTS_DIR=\\\"$$clean_path($$PWD/../../fonts)\\\"
DEFINES += CALAOS_LANG_DIR=\\\"$$clean_path($$PWD/../../lang)\\\"

# The resources tell whether the page reaches the kiosk.
RESOURCES += \
    $$PWD/../../img_desktop.qrc \
    $$PWD/../../qml_desktop.qrc

HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h \
    $$SRC_DIR/qqmlhelpers.h \
    $$SRC_DIR/VersionsModel.h \
    $$SRC_DIR/VersionsReport.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    $$SRC_DIR/VersionsModel.cpp \
    $$SRC_DIR/VersionsReport.cpp \
    tst_versionspage.cpp

OTHER_FILES += \
    tst_versionspage.qml
