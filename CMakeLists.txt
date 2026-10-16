cmake_minimum_required(VERSION 3.21)

project(CalaosHome LANGUAGES CXX)

# Objective-C/C++ is only needed for the iOS sources.
if(IOS)
    enable_language(OBJC OBJCXX)
endif()

# Guard: this CMake build is for mobile (iOS + Android) only.
# CMAKE_SYSTEM_NAME is resolved by project(), so check here.
if(NOT (IOS OR ANDROID))
    message(FATAL_ERROR
        "This CMakeLists.txt targets mobile builds only (iOS or Android).\n"
        "For the desktop variant: qmake desktop.pro")
endif()

include(cmake/Versioning.cmake)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Qml Quick Network Gui WebSockets Svg LinguistTools)

qt_standard_project_setup(REQUIRES 6.6)
set(CMAKE_AUTORCC ON)

# ── Third-party libraries ─────────────────────────────────────────────────────
set(quickflux_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(3rd_party/quickflux)
add_subdirectory(3rd_party/qt-qml-models)

# ── Shared C++ sources ────────────────────────────────────────────────────────
include(cmake/CalaosHomeSources.cmake)

# ── Main executable ───────────────────────────────────────────────────────────
if(ANDROID)
    qt_add_executable(CalaosHome MANUAL_FINALIZATION
        ${CALAOS_SOURCES} ${CALAOS_HEADERS}
        qml_mobile.qrc
        img_mobile.qrc
        fonts_mobile.qrc
        qml_shared.qrc
        lang.qrc
    )
else()
    qt_add_executable(CalaosHome MACOSX_BUNDLE
        ${CALAOS_SOURCES} ${CALAOS_HEADERS}
        qml_mobile.qrc
        img_mobile.qrc
        fonts_mobile.qrc
        qml_shared.qrc
        lang.qrc
    )
endif()

target_compile_definitions(CalaosHome PRIVATE CALAOS_MOBILE)

target_link_libraries(CalaosHome PRIVATE
    Qt6::Core
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Gui
    Qt6::WebSockets
    Qt6::Svg
    QuickFlux::quickflux
    qt-qml-models
)

# ── Translations ──────────────────────────────────────────────────────────────
# qt_add_translations LUPDATE_OPTIONS requires Qt 6.7+; safe with Qt 6.10.
qt_add_translations(CalaosHome
    TS_FILES lang/calaos_fr.ts lang/calaos_de.ts
    LUPDATE_OPTIONS -no-obsolete
)

# ── Platform-specific settings ────────────────────────────────────────────────
if(IOS)
    include(cmake/iOS.cmake)
elseif(ANDROID)
    include(cmake/Android.cmake)
    qt_finalize_executable(CalaosHome)
endif()
