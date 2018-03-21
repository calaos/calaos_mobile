QT += androidextras

OTHER_FILES += $$PWD/src/fr/calaos/calaoshome/HardwareUtils.java
ANDROID_PACKAGE_SOURCE_DIR = $$PWD

SOURCES += $$PWD/HardwareUtils_Android.cpp
HEADERS += $$PWD/HardwareUtils_Android.h

FIREBASE_SDK = $$(FIREBASE_CPP_SDK_DIR)
isEmpty(FIREBASE_SDK) {
    error("FIREBASE_CPP_SDK_DIR env is not set. Please set it first")
}

INCLUDEPATH += $$FIREBASE_SDK/include
LIBS += -L$$FIREBASE_SDK/libs/android/$$ANDROID_TARGET_ARCH/c++ \
        -lmessaging \
        -lapp \
        -llog \
        -lauth

ANDROID_EXTRA_LIBS += \
        $$PWD/openssl_prebuilt/$$ANDROID_TARGET_ARCH/libcrypto.so \
        $$PWD/openssl_prebuilt/$$ANDROID_TARGET_ARCH/libssl.so

DISTFILES += \
        $$PWD/res/drawable-hdpi/icon.png \
        $$PWD/res/drawable-ldpi/icon.png \
        $$PWD/res/drawable-mdpi/icon.png \
        $$PWD/google-services.json \
        $$PWD/AndroidManifest.xml \
        $$PWD/build.gradle \
        $$PWD/gradle/wrapper/gradle-wrapper.properties
