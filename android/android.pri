QT += gui-private

OTHER_FILES += $$PWD/src/fr/calaos/calaoshome/HardwareUtils.java \
               $$PWD/src/fr/calaos/calaoshome/CalaosMessagingService.java \
               $$PWD/src/fr/calaos/calaoshome/CredentialStore.java \
               $$PWD/src/fr/calaos/calaoshome/PushPictureWorker.java \
               $$PWD/debug/src/fr/calaos/calaoshome/debug/FakePushReceiver.java
ANDROID_PACKAGE_SOURCE_DIR = $$PWD

SOURCES += $$PWD/HardwareUtils_Android.cpp
HEADERS += $$PWD/HardwareUtils_Android.h

# Push notifications use the Firebase Java SDK (android/build.gradle), no native SDK.

include(C:/Users/$$(USERNAME)/AppData/Local/Android/Sdk/android_openssl/openssl.pri)

DISTFILES += \
        $$PWD/res/drawable-hdpi/icon.png \
        $$PWD/res/drawable-ldpi/icon.png \
        $$PWD/res/drawable-mdpi/icon.png \
        $$PWD/google-services.json \
        $$PWD/AndroidManifest.xml \
        $$PWD/build.gradle
