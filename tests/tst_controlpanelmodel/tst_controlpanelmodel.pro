TEMPLATE = app
TARGET = tst_controlpanelmodel

include(../common.pri)

# The config panel entries of the kiosk (src/ControlPanelModel.cpp): their
# translation, including a language change while the model lives.
QT += widgets

DEFINES += CALAOS_LANG_DIR=\\\"$$clean_path($$PWD/../../lang)\\\"

# Only to check that the icons exist in the resources.
RESOURCES += $$PWD/../../img_desktop.qrc

HEADERS += \
    $$SRC_DIR/ControlPanelModel.h \
    $$SRC_DIR/qqmlhelpers.h

SOURCES += \
    $$SRC_DIR/ControlPanelModel.cpp \
    tst_controlpanelmodel.cpp
