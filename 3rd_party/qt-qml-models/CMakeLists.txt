cmake_minimum_required(VERSION 3.21)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)

add_library(qt-qml-models STATIC
    QQmlObjectListModel.cpp
    QQmlObjectListModel.h
    QQmlVariantListModel.cpp
    QQmlVariantListModel.h
)

add_library(QtQmlModels::qt-qml-models ALIAS qt-qml-models)

set_target_properties(qt-qml-models PROPERTIES AUTOMOC TRUE)

target_include_directories(qt-qml-models
    PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include/qt-qml-models>"
)

target_link_libraries(qt-qml-models
    PUBLIC
    Qt6::Core
    Qt6::Qml
)
