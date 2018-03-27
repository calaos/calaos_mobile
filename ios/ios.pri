QMAKE_INFO_PLIST = $$PWD/AppInfo.plist
QMAKE_ASSET_CATALOGS = $$PWD/ressources.xcassets

#Force bundleIdentifier (PRODUCT_BUNDLE_IDENTIFIER) to be fr.calaos.CalaosMobile
CALAOS_BUNDLE.value = fr.calaos.CalaosMobile
CALAOS_BUNDLE.name = PRODUCT_BUNDLE_IDENTIFIER
QMAKE_MAC_XCODE_SETTINGS += CALAOS_BUNDLE

CALAOSDEVELOPMENT_TEAM.value = 9WYC46992U
CALAOSDEVELOPMENT_TEAM.name = DEVELOPMENT_TEAM
QMAKE_MAC_XCODE_SETTINGS += CALAOSDEVELOPMENT_TEAM

PUSH_ENTITLEMENTS.name = CODE_SIGN_ENTITLEMENTS
PUSH_ENTITLEMENTS.value = $$PWD/pushnotifications.entitlements
QMAKE_MAC_XCODE_SETTINGS += PUSH_ENTITLEMENTS

#Add framework for Reachability
LIBS += -framework SystemConfiguration -framework UserNotifications

HEADERS += $$PWD/HardwareUtils_iOS.h
OBJECTIVE_SOURCES += $$PWD/HardwareUtils.mm \
        $$PWD/Reachability.h \
        $$PWD/Reachability.m \
        $$PWD/SimpleKeychain/A0SimpleKeychain+KeyPair.h \
        $$PWD/SimpleKeychain/A0SimpleKeychain+KeyPair.m \
        $$PWD/SimpleKeychain/A0SimpleKeychain.h \
        $$PWD/SimpleKeychain/A0SimpleKeychain.m \
        $$PWD/SimpleKeychain/SimpleKeychain.h \
        $$PWD/AlertPrompt.h \
        $$PWD/AlertPrompt.m

OTHER_FILES += \
        $$PWD/Default-568h@2x.png \
        $$PWD/Icon-76.png \
        $$PWD/Icon-120.png \
        $$PWD/Icon-152.png \
        $$PWD/Default.png \
        $$PWD/Default@2x.png \
        $$PWD/Icon-72.png \
        $$PWD/Icon-Small-50.png \
        $$PWD/Icon-Small.png \
        $$PWD/Icon-Small@2x.png \
        $$PWD/Icon.png \
        $$PWD/Icon@2x.png \
        $$PWD/AppInfo.plist \
        $$PWD/pushnotifications.entitlements
