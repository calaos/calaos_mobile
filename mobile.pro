#Calaos Home for mobile

TEMPLATE = app

DEFINES += CALAOS_MOBILE

TARGET=CalaosHome

QT += qml quick network gui websockets svg

include(calaos.pri)
include(lang/lang.pri)

android {
    QT += androidextras
    OTHER_FILES += android/src/fr/calaos/calaoshome/HardwareUtils.java
    ANDROID_PACKAGE_SOURCE_DIR = $$PWD/android
    SOURCES += android/HardwareUtils_Android.cpp
    HEADERS += android/HardwareUtils_Android.h
    OTHER_FILES += \
        android/AndroidManifest.xml \
        android/res/drawable-hdpi/icon.png \
        android/res/drawable-ldpi/icon.png \
        android/res/drawable-mdpi/icon.png
} else:ios {
    QMAKE_INFO_PLIST = ios/AppInfo.plist
    QMAKE_ASSET_CATALOGS = $$PWD/ios/ressources.xcassets

    #Force bundleIdentifier (PRODUCT_BUNDLE_IDENTIFIER) to be fr.calaos.CalaosMobile
    CALAOS_BUNDLE.value = fr.calaos.CalaosMobile
    CALAOS_BUNDLE.name = PRODUCT_BUNDLE_IDENTIFIER
    QMAKE_MAC_XCODE_SETTINGS += CALAOS_BUNDLE

    CALAOSDEVELOPMENT_TEAM.value = 9WYC46992U
    CALAOSDEVELOPMENT_TEAM.name = DEVELOPMENT_TEAM
    QMAKE_MAC_XCODE_SETTINGS += CALAOSDEVELOPMENT_TEAM

    PUSH_ENTITLEMENTS.name = CODE_SIGN_ENTITLEMENTS
    PUSH_ENTITLEMENTS.value = $$PWD/ios/pushnotifications.entitlements
    QMAKE_MAC_XCODE_SETTINGS += PUSH_ENTITLEMENTS

    #Add framework for Reachability
    LIBS += -framework SystemConfiguration

    HEADERS += ios/HardwareUtils_iOS.h
    OBJECTIVE_SOURCES += ios/HardwareUtils.mm \
        ios/Reachability.h \
        ios/Reachability.m \
        ios/SimpleKeychain/A0SimpleKeychain+KeyPair.h \
        ios/SimpleKeychain/A0SimpleKeychain+KeyPair.m \
        ios/SimpleKeychain/A0SimpleKeychain.h \
        ios/SimpleKeychain/A0SimpleKeychain.m \
        ios/SimpleKeychain/SimpleKeychain.h \
        ios/AlertPrompt.h \
        ios/AlertPrompt.m

    OTHER_FILES += \
        ios/Default-568h@2x.png \
        ios/Icon-76.png \
        ios/Icon-120.png \
        ios/Icon-152.png \
        ios/Default.png \
        ios/Default@2x.png \
        ios/Icon-72.png \
        ios/Icon-Small-50.png \
        ios/Icon-Small.png \
        ios/Icon-Small@2x.png \
        ios/Icon.png \
        ios/Icon@2x.png \
        ios/AppInfo.plist \
        ios/pushnotifications.entitlements
} else {
    QT += widgets
}

RESOURCES += qml_mobile.qrc \
    img_mobile.qrc \
    fonts_mobile.qrc \
    qml_shared.qrc \
    lang.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Default rules for deployment.
include(deployment.pri)

contains(ANDROID_TARGET_ARCH,armeabi-v7a) {
    ANDROID_EXTRA_LIBS = \
        $$PWD/../openssl-1.0.2k/libcrypto.so \
        $$PWD/../openssl-1.0.2k/libssl.so
}
