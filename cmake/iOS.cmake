# cmake/iOS.cmake
# iOS-specific build settings.
# Included from the root CMakeLists.txt when IOS is set.

# ── Obj-C / Obj-C++ sources ───────────────────────────────────────────────────
target_sources(CalaosHome PRIVATE
    ios/HardwareUtils.mm
    ios/HardwareUtils_iOS.h
    ios/Reachability.h
    ios/Reachability.m
    ios/SimpleKeychain/SimpleKeychain.h
    ios/SimpleKeychain/A0SimpleKeychain.h
    ios/SimpleKeychain/A0SimpleKeychain.m
    ios/SimpleKeychain/A0SimpleKeychain+KeyPair.h
    ios/SimpleKeychain/A0SimpleKeychain+KeyPair.m
)

# Force .m files to compile as Objective-C (not plain C) and .mm as Objective-C++.
# Without this the Xcode generator may misidentify the language when AUTOMOC is active.
set_source_files_properties(
    ios/Reachability.m
    ios/SimpleKeychain/A0SimpleKeychain.m
    "ios/SimpleKeychain/A0SimpleKeychain+KeyPair.m"
    PROPERTIES LANGUAGE OBJC
)
set_source_files_properties(
    ios/HardwareUtils.mm
    PROPERTIES LANGUAGE OBJCXX
)

# ── Asset catalog ─────────────────────────────────────────────────────────────
# Explicitly mark the xcassets as a Resource so CMake places it in the
# "Copy Bundle Resources" build phase — Xcode then runs actool on it.
set_source_files_properties(ios/ressources.xcassets PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
target_sources(CalaosHome PRIVATE ios/ressources.xcassets)

# ── Launch storyboard ─────────────────────────────────────────────────────────
set_source_files_properties(ios/Splash.storyboard PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
target_sources(CalaosHome PRIVATE ios/Splash.storyboard)

# ── Frameworks ────────────────────────────────────────────────────────────────
target_link_libraries(CalaosHome PRIVATE
    "-framework SystemConfiguration"
    "-framework UserNotifications"
)

# Qt gui-private is needed by the iOS HardwareUtils delegate overrides
target_link_libraries(CalaosHome PRIVATE Qt6::GuiPrivate)

# Include path: NotificationService.m imports "ios/SimpleKeychain/..." relative
# to the repo root, so add CMAKE_SOURCE_DIR as an include directory.
target_include_directories(CalaosHome PRIVATE "${CMAKE_SOURCE_DIR}")

# ── Xcode target attributes ───────────────────────────────────────────────────
set_target_properties(CalaosHome PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST                           "${CMAKE_SOURCE_DIR}/ios/AppInfo.plist"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER          "fr.calaos.CalaosMobile"
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM                   "9WYC46992U"
    XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS             "${CMAKE_SOURCE_DIR}/ios/pushnotifications.entitlements"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE                    "Automatic"
    XCODE_ATTRIBUTE_ENABLE_BITCODE                     "NO"
    XCODE_ATTRIBUTE_MARKETING_VERSION                  "${CALAOS_VERSION}"
    XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION            "${CALAOS_VERSION_CODE}"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY             "1,2"
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET         "17.0"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    # Produce a dSYM bundle in Release/archive builds so symbols can be uploaded
    # (App Store / crash symbolication). CMake's Xcode generator otherwise leaves
    # this at "dwarf", which yields no dSYM.
    "XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release]" "dwarf-with-dsym"
)

# ── PushNotif Notification Service Extension ──────────────────────────────────
add_subdirectory(ios)

# Embed the extension into the main app bundle
set_target_properties(CalaosHome PROPERTIES
    XCODE_EMBED_APP_EXTENSIONS                         "PushNotif"
    XCODE_EMBED_APP_EXTENSIONS_CODE_SIGN_ON_COPY       "YES"
    XCODE_EMBED_APP_EXTENSIONS_REMOVE_HEADERS_ON_COPY  "YES"
)

# ── Archive custom target ─────────────────────────────────────────────────────
set(_XCODEPROJ  "${CMAKE_BINARY_DIR}/CalaosHome.xcodeproj")
set(_ARCHIVE    "${CMAKE_BINARY_DIR}/CalaosMobile.xcarchive")
set(_EXPORT_DIR "${CMAKE_BINARY_DIR}/export")

# dSYM bundles are emitted into the CMake build tree (DWARF_DSYM_FOLDER_PATH is
# redirected per target), so xcodebuild does not collect them into the archive's
# dSYMs/ folder. Copy them in explicitly after archiving so symbol upload works.
set(_DSYM_APP "${CMAKE_BINARY_DIR}/Release-iphoneos/CalaosHome.app.dSYM")
set(_DSYM_EXT "${CMAKE_BINARY_DIR}/ios/Release-iphoneos/PushNotif.appex.dSYM")

add_custom_target(archive
    COMMAND xcodebuild
        -project    "${_XCODEPROJ}"
        -scheme     CalaosHome
        -configuration Release
        -destination "generic/platform=iOS"
        -archivePath "${_ARCHIVE}"
        -allowProvisioningUpdates
        archive
    # Populate the archive's dSYMs/ folder from the build-tree dSYMs.
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${_DSYM_APP}" "${_ARCHIVE}/dSYMs/CalaosHome.app.dSYM"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${_DSYM_EXT}" "${_ARCHIVE}/dSYMs/PushNotif.appex.dSYM"
    COMMAND xcodebuild
        -exportArchive
        -archivePath    "${_ARCHIVE}"
        -exportOptionsPlist "${CMAKE_SOURCE_DIR}/ios/ExportOptions.plist"
        -exportPath     "${_EXPORT_DIR}"
        -allowProvisioningUpdates
    COMMENT "Archiving CalaosHome and exporting .ipa to ${_EXPORT_DIR}"
    VERBATIM
)
