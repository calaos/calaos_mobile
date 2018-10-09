#Calaos Home for mobile

TEMPLATE = app

DEFINES += CALAOS_MOBILE

TARGET=CalaosHome

QT += qml quick network gui websockets svg

include(calaos.pri)
include(lang/lang.pri)

android {
    include(android/android.pri)
} else:ios {
    include(ios/ios.pri)
} else {
    QT += widgets
}

RESOURCES += qml_mobile.qrc \
    img_mobile.qrc \
    fonts_mobile.qrc \
    qml_shared.qrc \
    lang.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH = $$PWD/qml

# Default rules for deployment.
include(deployment.pri)

contains(ANDROID_TARGET_ARCH,armeabi-v7a) {
    ANDROID_EXTRA_LIBS = \
        $$PWD/../openssl-1.0.2k/libcrypto.so \
        $$PWD/../openssl-1.0.2k/libssl.so
}
