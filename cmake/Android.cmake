# cmake/Android.cmake
# Android-specific build settings.
# Included from the root CMakeLists.txt when ANDROID is set.

# ── Options ───────────────────────────────────────────────────────────────────
# Push notifications (FCM) live entirely on the Java/Gradle side
# (android/build.gradle, CalaosMessagingService.java): nothing to enable here.

# ABIs to build. Debug uses arm64-v8a only; release multi-ABI comes with AND-5.
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
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_MIN_SDK_VERSION 28)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_TARGET_SDK_VERSION 36)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_ABIS "${CALAOS_ANDROID_ABIS}")

# Application.cpp includes <QtGui/qpa/qplatformwindow.h> (private header).
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
target_link_libraries(CalaosHome PRIVATE Qt6::GuiPrivate)
