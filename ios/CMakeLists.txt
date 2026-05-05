# ios/CMakeLists.txt
# Defines the PushNotif Notification Service Extension target.
# The extension must NOT link any Qt library.

add_executable(PushNotif MACOSX_BUNDLE
    PushNotif/NotificationService.h
    PushNotif/NotificationService.m
    # SimpleKeychain sources — copies in ios/PushNotif/ mirror ios/SimpleKeychain/.
    PushNotif/A0SimpleKeychain.h
    PushNotif/A0SimpleKeychain.m
    "PushNotif/A0SimpleKeychain+KeyPair.h"
    "PushNotif/A0SimpleKeychain+KeyPair.m"
)

# Explicitly mark .m files as Objective-C so the Xcode generator sets the
# correct language type (objective-c, not c). Without this, qt_standard_project_setup()'s
# global AUTOMOC can cause the Xcode generator to misidentify the language.
set_source_files_properties(
    PushNotif/NotificationService.m
    PushNotif/A0SimpleKeychain.m
    "PushNotif/A0SimpleKeychain+KeyPair.m"
    PROPERTIES LANGUAGE OBJC
)

# AUTOMOC must be off — PushNotif has no Qt dependency and no Qt headers.
set_target_properties(PushNotif PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)

# Include path so that NotificationService.m's #import "ios/SimpleKeychain/..."
# resolves against the repo root.
target_include_directories(PushNotif PRIVATE "${CMAKE_SOURCE_DIR}")

# Frameworks only — absolutely no Qt::* links.
# Security is used by A0SimpleKeychain (SecItemAdd, SecAccessControlCreateWithFlags, etc.)
target_link_libraries(PushNotif PRIVATE
    "-framework Foundation"
    "-framework UserNotifications"
    "-framework Security"
)

set_target_properties(PushNotif PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST
        "${CMAKE_CURRENT_SOURCE_DIR}/PushNotif/Info.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER
        "fr.calaos.CalaosMobile.PushNotif"
    XCODE_PRODUCT_TYPE
        "com.apple.product-type.app-extension"
    XCODE_ATTRIBUTE_INFOPLIST_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/PushNotif/Info.plist"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS
        "${CMAKE_CURRENT_SOURCE_DIR}/PushNotif/PushNotif.entitlements"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM
        "9WYC46992U"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE
        "Automatic"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC
        "YES"
    # -rdynamic exports all global symbols dynamically; required for the new Xcode 15+ ObjC
    # _objc_msgSend$ stub synthesis to resolve intra-binary method dispatch.
    # The original PushNotif.xcodeproj has this flag auto-added by Xcode for app extensions.
    XCODE_ATTRIBUTE_OTHER_LDFLAGS
        "-rdynamic"
    XCODE_ATTRIBUTE_ENABLE_BITCODE
        "NO"
    XCODE_ATTRIBUTE_SKIP_INSTALL
        "YES"
    XCODE_ATTRIBUTE_MARKETING_VERSION
        "${CALAOS_VERSION}"
    XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION
        "${CALAOS_VERSION_CODE}"
)
