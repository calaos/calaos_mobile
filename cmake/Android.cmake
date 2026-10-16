# cmake/Android.cmake
# Android-specific build settings.
# Included from the root CMakeLists.txt when ANDROID is set.

# ── Firebase C++ SDK ──────────────────────────────────────────────────────────
if(NOT DEFINED ENV{FIREBASE_CPP_SDK_DIR})
    message(FATAL_ERROR
        "FIREBASE_CPP_SDK_DIR environment variable is not set.\n"
        "Download the Firebase C++ SDK from https://firebase.google.com/download/cpp\n"
        "then: export FIREBASE_CPP_SDK_DIR=/path/to/firebase_cpp_sdk")
endif()

set(FIREBASE_SDK "$ENV{FIREBASE_CPP_SDK_DIR}")

target_include_directories(CalaosHome PRIVATE "${FIREBASE_SDK}/include")
target_link_directories(CalaosHome PRIVATE
    "${FIREBASE_SDK}/libs/android/${ANDROID_ABI}")
target_link_libraries(CalaosHome PRIVATE
    firebase_messaging
    firebase_app
    firebase_auth
)

# ── Android OpenSSL prebuilt (KDAB android_openssl) ──────────────────────────
if(NOT DEFINED ANDROID_OPENSSL_DIR)
    set(ANDROID_OPENSSL_DIR "$ENV{HOME}/Android/Sdk/android_openssl"
        CACHE PATH "KDAB android_openssl prebuilt root")
endif()

set(_openssl_cmake "${ANDROID_OPENSSL_DIR}/android_openssl.cmake")
if(NOT EXISTS "${_openssl_cmake}")
    message(FATAL_ERROR
        "KDAB android_openssl cmake file not found: ${_openssl_cmake}\n"
        "Clone https://github.com/KDAB/android_openssl to ${ANDROID_OPENSSL_DIR}\n"
        "or pass -DANDROID_OPENSSL_DIR=/custom/path to cmake.")
endif()

include("${_openssl_cmake}")

# KDAB's android_openssl.cmake may provide add_android_openssl_libraries() (newer) or
# populate ANDROID_EXTRA_OPENSSL_LIBS (older). Handle both.
if(COMMAND add_android_openssl_libraries)
    add_android_openssl_libraries(CalaosHome)
elseif(DEFINED ANDROID_EXTRA_OPENSSL_LIBS)
    set_property(TARGET CalaosHome APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS ${ANDROID_EXTRA_OPENSSL_LIBS})
endif()

# ── Android-specific sources ──────────────────────────────────────────────────
target_sources(CalaosHome PRIVATE
    android/HardwareUtils_Android.cpp
    android/HardwareUtils_Android.h
)

# Qt gui-private is needed by Application.cpp (qpa/qplatformwindow.h)
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
target_link_libraries(CalaosHome PRIVATE Qt6::GuiPrivate)

# ── Android target properties ─────────────────────────────────────────────────
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_VERSION_NAME "${CALAOS_VERSION}")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_VERSION_CODE "${CALAOS_VERSION_CODE}")
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_MIN_SDK_VERSION 26)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_TARGET_SDK_VERSION 34)
set_property(TARGET CalaosHome PROPERTY
    QT_ANDROID_ABIS "armeabi-v7a;arm64-v8a;x86;x86_64")
