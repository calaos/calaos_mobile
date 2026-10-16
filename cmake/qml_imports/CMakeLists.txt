# cmake/qml_imports/CMakeLists.txt
# QML plugin import for the static (iOS) build. Added from cmake/iOS.cmake.
#
# With a static Qt, qt_import_qml_plugins() runs qmlimportscanner with -rootPath
# set to the target's source directory and links a QML plugin for every import it
# finds. For CalaosHome that directory is the repository root, so the desktop QML,
# the QML tests, the desktop widgets and the 3rd-party examples were scanned too
# (VirtualKeyboard, QtTest, ... linked into the app); qmlimportscanner's -exclude
# option does not help. Qt 6.12 turns this into a CMake generate error: its static
# VirtualKeyboard components plugin links Qt6::Multimedia, which the kit lacks.
#
# This INTERFACE target lives in a directory without QML, so the scan only covers
# the root paths below: the QML that the mobile .qrc files embed.
add_library(CalaosHomeQmlImports INTERFACE)
# Qt treats any library passed to qt_import_qml_plugins() as a QML module and warns
# unless its output directory ends with its target path: give it a consistent pair.
set_target_properties(CalaosHomeQmlImports PROPERTIES
    QT_QML_MODULE_TARGET_PATH "CalaosHomeQmlImports"
    QT_QML_MODULE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CalaosHomeQmlImports"
    QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS
        "${CMAKE_SOURCE_DIR}/qml/mobile;${CMAKE_SOURCE_DIR}/qml/SharedComponents;${CMAKE_SOURCE_DIR}/qml/quickflux"
)
qt_import_qml_plugins(CalaosHomeQmlImports)
