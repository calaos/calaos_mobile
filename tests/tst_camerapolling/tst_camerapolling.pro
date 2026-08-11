TEMPLATE = app
TARGET = tst_camerapolling

include(../common.pri)

# CameraModel.cpp includes qfappdispatcher.h and calls QFAppDispatcher in
# eventTouchscreenCamera(), a private slot: moc references it from the static
# metacall table however narrow the test is, so quickflux must be linked even
# though this test never dispatches anything.
include(../../3rd_party/quickflux/quickflux.pri)

# CALAOS_MOBILE (rather than CALAOS_DESKTOP) keeps ScreenManager out of the
# link: CameraModel::eventTouchscreenCamera() only calls it under
# CALAOS_DESKTOP. HardwareUtils::Instance() then returns the plain base class,
# whose methods are no-ops - exactly what a headless test wants.
DEFINES += CALAOS_MOBILE

# HardwareUtils.cpp includes <QInputDialog> outside iOS/Android, and
# QQmlApplicationEngine needs a QGuiApplication; QApplication covers both.
QT += widgets quick

HEADERS += \
    $$SRC_DIR/CameraModel.h \
    $$SRC_DIR/ModelImageProvider.h \
    $$SRC_DIR/CalaosConnection.h \
    $$SRC_DIR/HardwareUtils.h \
    $$SRC_DIR/Common.h \
    $$SRC_DIR/IOTypeRegistry.h

SOURCES += \
    $$SRC_DIR/CameraModel.cpp \
    $$SRC_DIR/ModelImageProvider.cpp \
    $$SRC_DIR/CalaosConnection.cpp \
    $$SRC_DIR/CalaosEventDecoder.cpp \
    $$SRC_DIR/HardwareUtils.cpp \
    $$SRC_DIR/Common.cpp \
    $$SRC_DIR/IOTypeRegistry.cpp \
    tst_camerapolling.cpp
