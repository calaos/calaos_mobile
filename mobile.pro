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
#android: include(C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/openssl.pri)

#ANDROID_EXTRA_LIBS = C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/arm/libcrypto_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/arm/libssl_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/arm64/libcrypto_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/arm64/libssl_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/x86/libcrypto_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/x86/libssl_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/x86_64/libcrypto_1_1.so C:/Users/raoul/AppData/Local/Android/Sdk/android_openssl/no-asm/latest/x86_64/libssl_1_1.so

DISTFILES += \
    android/gradle/wrapper/gradle-wrapper.jar \
    android/gradlew \
    android/gradlew.bat \
    android/res/values/libs.xml
