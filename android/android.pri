QT += gui-private

OTHER_FILES += $$PWD/src/fr/calaos/calaoshome/HardwareUtils.java
ANDROID_PACKAGE_SOURCE_DIR = $$PWD

SOURCES += $$PWD/HardwareUtils_Android.cpp
HEADERS += $$PWD/HardwareUtils_Android.h

FIREBASE_SDK = $$(FIREBASE_CPP_SDK_DIR)
isEmpty(FIREBASE_SDK) {
    error("FIREBASE_CPP_SDK_DIR env is not set. Please set it first")
}

INCLUDEPATH += $$FIREBASE_SDK/include
LIBS += -L$$FIREBASE_SDK/libs/android/$$ANDROID_TARGET_ARCH \
        -lfirebase_messaging \
        -lfirebase_app \
        -lfirebase_auth

include(C:/Users/$$(USERNAME)/AppData/Local/Android/Sdk/android_openssl/openssl.pri)

DISTFILES += \
        $$PWD/gradle.properties \
        $$PWD/res/drawable-hdpi/icon.png \
        $$PWD/res/drawable-ldpi/icon.png \
        $$PWD/res/drawable-mdpi/icon.png \
        $$PWD/google-services.json \
        $$PWD/AndroidManifest.xml \
        $$PWD/build.gradle \
        $$PWD/gradle/wrapper/gradle-wrapper.properties \
        $$PWD/settings.gradle
