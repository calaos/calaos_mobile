TEMPLATE = app
TARGET = tst_eventspage

include(../common.pri)

# Qt Quick Test of the event log page of the kiosk (qml/desktop/ConfigEventsView.qml)
# and of its entry in the config panel, on a stand-in for eventLogModel; the
# QuickFlux stub is the one of tst_climatewidget.
QT += quick qmltest widgets
CONFIG += qmltestcase

DEFINES += QUICK_TEST_SOURCE_DIR=\\\"$$PWD\\\"
DEFINES += CALAOS_STUB_QML_DIR=\\\"$$clean_path($$PWD/../tst_climatewidget/stubs)\\\"
DEFINES += CALAOS_QML_DIR=\\\"$$clean_path($$PWD/../../qml)\\\"
DEFINES += CALAOS_FONTS_DIR=\\\"$$clean_path($$PWD/../../fonts)\\\"
DEFINES += CALAOS_LANG_DIR=\\\"$$clean_path($$PWD/../../lang)\\\"

# The resources tell whether the page and its icon reach the kiosk.
RESOURCES += \
    $$PWD/../../img_desktop.qrc \
    $$PWD/../../qml_desktop.qrc

HEADERS += \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/ControlPanelModel.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/ControlPanelModel.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_eventspage.cpp

OTHER_FILES += \
    tst_eventspage.qml
