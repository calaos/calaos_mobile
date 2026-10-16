# cmake/Android.cmake
# Android-specific build settings.
# Included from the root CMakeLists.txt when ANDROID is set.

# ── Options ───────────────────────────────────────────────────────────────────
# Push notifications (FCM) live entirely on the Java/Gradle side
# (android/build.gradle, CalaosMessagingService.java): nothing to enable here.

# ABIs to build (set by tools/build_android.sh: arm64-v8a for Debug, all four for Release).
set(CALAOS_ANDROID_ABIS "arm64-v8a" CACHE STRING
    "Semicolon-separated Android ABIs to build")

# ── OpenSSL (KDAB android_openssl) ────────────────────────────────────────────
if(NOT ANDROID_OPENSSL_DIR)
    if(DEFINED ENV{ANDROID_SDK_ROOT})
        set(_sdk_root "$ENV{ANDROID_SDK_ROOT}")
    else()
        set(_sdk_root "$ENV{HOME}/Library/Android/sdk")
    endif()
    set(ANDROID_OPENSSL_DIR "${_sdk_root}/android_openssl"
        CACHE PATH "KDAB android_openssl root (contains android_openssl.cmake)")
endif()

set(_openssl_cmake "${ANDROID_OPENSSL_DIR}/android_openssl.cmake")
if(NOT EXISTS "${_openssl_cmake}")
    message(FATAL_ERROR
        "KDAB android_openssl cmake file not found: ${_openssl_cmake}\n"
        "Clone https://github.com/KDAB/android_openssl to ${ANDROID_OPENSSL_DIR}\n"
        "or pass -DANDROID_OPENSSL_DIR=/custom/path to cmake.")
endif()

# Defines OpenSSL::SSL / OpenSSL::Crypto and add_android_openssl_libraries().
# It picks ssl_3 for Qt >= 6.5, and the no-asm variant when CMAKE_BUILD_TYPE=Debug.
include("${_openssl_cmake}")
add_android_openssl_libraries(CalaosHome)

# ── Android-specific sources ──────────────────────────────────────────────────
target_sources(CalaosHome PRIVATE
    android/HardwareUtils_Android.cpp
    android/HardwareUtils_Android.h
)

# ── Android target properties ─────────────────────────────────────────────────
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_VERSION_NAME "${CALAOS_VERSION}")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_VERSION_CODE "${CALAOS_VERSION_CODE}")
# Compile SDK pinned to the one Qt 6.12 builds its Java against
# (QT_ANDROID_API_USED_FOR_JAVA). From API 37 on, platforms carry a minor level:
# use "37.0", "37" matches no installed platform.
set(CALAOS_ANDROID_COMPILE_SDK "37.0")
if(DEFINED ANDROID_SDK_ROOT)
    set(_calaos_sdk_root "${ANDROID_SDK_ROOT}")
elseif(DEFINED ENV{ANDROID_SDK_ROOT})
    set(_calaos_sdk_root "$ENV{ANDROID_SDK_ROOT}")
else()
    set(_calaos_sdk_root "$ENV{HOME}/Library/Android/sdk")
endif()
if(NOT EXISTS "${_calaos_sdk_root}/platforms/android-${CALAOS_ANDROID_COMPILE_SDK}/android.jar")
    message(FATAL_ERROR
        "Android SDK platform android-${CALAOS_ANDROID_COMPILE_SDK} not found in "
        "${_calaos_sdk_root}/platforms. Install it with:\n"
        "  sdkmanager \"platforms;android-${CALAOS_ANDROID_COMPILE_SDK}\"")
endif()
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_COMPILE_SDK_VERSION "${CALAOS_ANDROID_COMPILE_SDK}")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_MIN_SDK_VERSION 28)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_TARGET_SDK_VERSION 36)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_ABIS "${CALAOS_ANDROID_ABIS}")


# ── Deployed Qt plugins ───────────────────────────────────────────────────────
# androiddeployqt packages every plugin of the linked Qt modules (it cannot tell
# which ones load at run time), for each ABI. Leave out those this app never
# uses. Kept on purpose: the Android platform plugin, the OpenSSL TLS backend,
# the Android network-information backend, and the jpeg, gif, svg and webp
# image formats (camera snapshots, music covers and the app's own images).
# QML debugging plugins (qmltooling) stay in Debug builds for Qt Creator.
set(_calaos_excluded_plugin_types platforminputcontexts iconengines)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND _calaos_excluded_plugin_types qmltooling)
endif()
qt_import_plugins(CalaosHome
    EXCLUDE_BY_TYPE ${_calaos_excluded_plugin_types}
    EXCLUDE
        Qt6::QOffscreenIntegrationPlugin
        Qt6::QICNSPlugin
        Qt6::QICOPlugin
        Qt6::QTgaPlugin
        Qt6::QTiffPlugin
        Qt6::QWbmpPlugin
)

# ── Deployed QML modules ──────────────────────────────────────────────────────
# androiddeployqt runs qmlimportscanner over the QML root paths and packages
# (and loads at startup) every QML module it finds. Qt always adds the target's
# source directory, i.e. the repository root, so the desktop QML, the QML tests
# and the 3rd-party examples were scanned too (VirtualKeyboard, QtTest,
# Dialogs, FolderListModel... in the APK). Call this after
# qt_finalize_executable(): it replaces the generated list with the QML that the
# mobile .qrc files embed (same list as cmake/qml_imports for iOS: keep both in
# sync with qml_mobile.qrc and qml_shared.qrc). The deployment settings read
# this internal property through a generator expression; if a Qt upgrade drops
# it the scan falls back to the whole repository (bigger APK, nothing missing).
function(calaos_android_limit_qml_scan target)
    get_target_property(_roots ${target} _qt_android_native_qml_root_paths)
    if(NOT _roots)
        message(WARNING "Qt no longer sets _qt_android_native_qml_root_paths: "
            "the Android QML import scan covers the whole repository again")
        return()
    endif()
    set_property(TARGET ${target} PROPERTY _qt_android_native_qml_root_paths
        "${CMAKE_SOURCE_DIR}/qml/mobile"
        "${CMAKE_SOURCE_DIR}/qml/SharedComponents"
        "${CMAKE_SOURCE_DIR}/qml/quickflux")
endfunction()
